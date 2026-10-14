// No engine PCH: some legacy libraries include device.h with a pre-Vista
// _WIN32_WINNT. Keep the native API requirement local to this implementation.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "LuaGCController.h"
#include <new>

struct NativeWorkerPool::State
{
	PTP_POOL pool = nullptr;
	TP_CALLBACK_ENVIRON environment{};
	State() { InitializeThreadpoolEnvironment(&environment); }
	~State()
	{
		DestroyThreadpoolEnvironment(&environment);
		if (pool) CloseThreadpool(pool); // All bound work handles have already joined/closed.
	}
};

bool NativeWorkerPool::Initialize()
{
	if (state) return true;
	if (!limit) return false;
	auto* next = new (std::nothrow) State();
	if (!next) return false;
	next->pool = CreateThreadpool(nullptr);
	if (!next->pool) { delete next; return false; }
	SetThreadpoolThreadMaximum(next->pool, limit);
	// Retain the small proven owner set instead of creating/reaping frame
	// executors alongside unrelated users of the process-default Windows pool.
	if (!SetThreadpoolThreadMinimum(next->pool, limit)) { delete next; return false; }
	SetThreadpoolCallbackPool(&next->environment, next->pool);
	state = next;
	return true;
}

NativeWorkerPool::~NativeWorkerPool() { delete state; }

void CALLBACK LuaGCFrameWorker::Invoke(PTP_CALLBACK_INSTANCE, void* context, PTP_WORK)
{
	auto& self = *static_cast<LuaGCFrameWorker*>(context);
	try { self.function(self.context); }
	catch (...) { self.error = std::current_exception(); }
}

LuaGCFrameWorker::~LuaGCFrameWorker()
{
	// Destructors must not rethrow a task failure during process teardown.
	if (!work) return;
	WaitForThreadpoolWorkCallbacks(work, FALSE);
	CloseThreadpoolWork(work);
}

bool LuaGCFrameWorker::Initialize(Function callback, void* argument, NativeWorkerPool& pool)
{
	if (work) return true;
	if (!pool.Initialize()) return false;
	function = callback;
	context = argument;
	work = CreateThreadpoolWork(Invoke, this, &pool.state->environment);
	return work != nullptr;
}

void LuaGCFrameWorker::Start()
{
	if (!work) throw std::logic_error("Lua/game-logic worker is not initialized");
	bool expected = false;
	if (!running.compare_exchange_strong(expected, true))
		throw std::logic_error("Lua/game-logic worker is already running");
	error = nullptr;
	SubmitThreadpoolWork(work);
}

bool LuaGCFrameWorker::Wait()
{
	if (!running.load()) return false;
	WaitForThreadpoolWorkCallbacks(work, FALSE);
	running.store(false);
	if (error) std::rethrow_exception(error);
	return true;
}

void LuaGCFrameWorker::Shutdown()
{
	if (!work) return;
	Wait();
	CloseThreadpoolWork(work);
	work = nullptr;
}
