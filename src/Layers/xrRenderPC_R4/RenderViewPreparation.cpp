#include "stdafx.h"
#include "RenderViewPreparation.h"
#include "../xrRender/FHierrarhyVisual.h"
#include "../xrRender/FVisual.h"
#include "../xrRender/FTreeVisual.h"
#include <atomic>
#include <ppl.h>
#include <unordered_map>

namespace
{
constexpr u32 no_index = u32(-1), max_nodes = 131072, max_views = 512, max_commands = 32768;
constexpr size_t graph_memory_limit = 64u * 1024u * 1024u;

struct NodeInput
{
    dxRender_Visual* owner = nullptr;
    PreparedStaticCommand::Kind kind = PreparedStaticCommand::Fallback;
    u32 type = 0;
    Fbox bounds;
    Fsphere sphere;
    std::vector<u32> children;
    bool tree = false;
    Fmatrix transform;
};
struct SectorInput { CSector* owner; u32 root; std::vector<u32> portals; };
struct PortalInput
{
    CPortal* owner;
    Fplane plane;
    Fsphere sphere;
    svector<Fvector, 8> polygon;
    u32 front, back;
};
struct SceneInput
{
    std::vector<NodeInput> nodes;
    std::vector<SectorInput> sectors;
    std::vector<PortalInput> portals;
    std::unordered_map<CSector*, u32> sector_ids;
};

// This is the current owner's portal clipping math, executed on copied polygons.
CFrustum PreparationFrustum(sPoly* poly, Fvector& vBase, Fmatrix& mFullXFORM)
{
    CFrustum F;
    Fplane P;
    P.build_precise ((*poly)[0],(*poly)[1],(*poly)[2]);

    if (poly->size() > 6)
    {
        F.SimplifyPoly_AABB(poly,P);
        P.build_precise ((*poly)[0],(*poly)[1],(*poly)[2]);
    }

    // Check plane orientation relative to viewer
    // and reverse if needed
    if (P.classify(vBase)<0)
    {
        std::reverse(poly->begin(),poly->end());
        P.build_precise ((*poly)[0],(*poly)[1],(*poly)[2]);
    }

    // Base creation
    F.CreateFromPoints(poly->begin(),poly->size(),vBase);

    // Near clipping plane
    F._add      (P);

    // Far clipping plane
    Fmatrix &M  = mFullXFORM;
    P.n.x       = -(M._14 - M._13);
    P.n.y       = -(M._24 - M._23);
    P.n.z       = -(M._34 - M._33);
    P.d         = -(M._44 - M._43);
    float denom = 1.0f / P.n.magnitude();
    P.n.x       *= denom;
    P.n.y       *= denom;
    P.n.z       *= denom;
    P.d         *= denom;
    F._add      (P);

    return F;
}



struct ViewJob
{
    struct TreeOutput { Fmatrix world, combined; };
    struct Output
    {
        u32 sector;
        CFrustum view;
        std::vector<PreparedStaticCommand> commands;
    };
    u32 start = 0, commands = 0;
    bool camera = false, valid = false;
    CFrustum view;
    Fmatrix transform, render_view;
    Fvector origin;
    std::vector<u8> dual, visited;
    std::vector<Output> outputs;
    std::vector<TreeOutput> trees;
    std::atomic<bool> ready{false};

    void Geometry(const SceneInput& scene, u32 id, const CFrustum& frustum,
        u32 planes, bool full, std::vector<PreparedStaticCommand>& output, u32 depth = 0)
    {
        if (!valid || id == no_index) return;
        if (depth >= 64)
        {
            valid = false;
            return;
        }
        const auto& node = scene.nodes[id];
        u32 mask = planes;
        if (node.kind != PreparedStaticCommand::Fallback)
        {
            Fvector center = node.sphere.P;
            const auto hit = full ? fcvFully : frustum.testSAABB(center, node.sphere.R, node.bounds.data(), mask);
            if (hit == fcvNone) return;
            full = hit == fcvFully;
        }
        if (++commands > max_commands)
        {
            valid = false;
            return;
        }
        const u32 begin = u32(output.size());
        output.push_back({node.kind, node.owner, 0, mask, full});
        if (node.tree)
        {
            TreeOutput tree;
            tree.world = node.transform;
            tree.combined.mul_43(render_view, tree.world);
            output.back().tree = u32(trees.size());
            trees.push_back(tree);
        }
        if (node.kind == PreparedStaticCommand::Hierarchy)
            for (u32 child : node.children) Geometry(scene, child, frustum, mask, full, output, depth + 1);
        output[begin].end = u32(output.size());
    }

    void PortalWalk(const SceneInput& scene, u32 sector, const CFrustum& frustum, u32 depth)
    {
        if (!valid) return;
        if (depth >= 128 || outputs.size() >= max_views)
        {
            valid = false;
            return;
        }
        const u32 index = u32(outputs.size());
        outputs.push_back({sector, frustum, {}});
        Geometry(scene, scene.sectors[sector].root, frustum, frustum.getMask(), false, outputs[index].commands);
        for (u32 id : scene.sectors[sector].portals)
        {
            if (!valid) return;
            if (visited[id]) continue;
            const auto& portal = scene.portals[id];
            const u32 next = dual[id] ? (portal.front == sector ? portal.back : portal.front) :
                (portal.plane.classify(origin) > 0 ? portal.back : portal.front);
            if (next == no_index || (!dual[id] && (next == sector || next == start))) continue;
            Fvector center = portal.sphere.P;
            if (!frustum.testSphere_dirty(center, portal.sphere.R)) continue;
            sPoly source, destination;
            source.clear();
            destination.clear();
            for (const auto& point : portal.polygon) source.push_back(point);
            sPoly* polygon = frustum.ClipPoly(source, destination);
            if (!polygon) continue;
            CFrustum clip = PreparationFrustum(polygon, origin, transform);
            visited[id] = 1;
            dual[id] = 0;
            PortalWalk(scene, next, clip, depth + 1);
        }
    }
    void Compute(const SceneInput& scene)
    {
        PROF_EVENT("Render.Views.Compute");
        commands = 0;
        valid = true;
        outputs.clear();
        trees.clear();
        visited.assign(scene.portals.size(), 0);
        PortalWalk(scene, start, view, 0);
        ready.store(true, std::memory_order_release);
    }
};

bool SamePacket(const R_dsgraph::RenderPacket& left, const R_dsgraph::RenderPacket& right)
{
    return left.sortKey == right.sortKey && left.pVS == right.pVS && left.pPS == right.pPS &&
        left.pGS == right.pGS && left.pHS == right.pHS && left.pDS == right.pDS &&
        left.pCS == right.pCS && left.pState == right.pState && left.pTextures == right.pTextures &&
        left.source_pass == right.source_pass && left.static_batch == right.static_batch &&
        left.static_first_index == right.static_first_index && left.item.pVisual == right.item.pVisual &&
        left.item.pObject == right.item.pObject && left.item.pMatrix == right.item.pMatrix &&
        left.item.b_hud_mode == right.item.b_hud_mode && !memcmp(&left.item.ssa, &right.item.ssa, sizeof(float));
}

struct GraphJob : PreparedGraphMatrices
{
    R_dsgraph::RenderQueue* target = nullptr; // Owner-only lookup identity.
    R_dsgraph::RenderQueue input, output;
    std::vector<Fmatrix> worlds;
    bool dynamic = false;
    u32 phase = 0;
    std::atomic<bool> ready{false}, cancelled{false};
    size_t Bytes() const
    {
        return (input.capacity() + output.capacity()) * sizeof(R_dsgraph::RenderPacket) +
            worlds.capacity() * sizeof(Fmatrix) + matrices.capacity() * sizeof(PreparedGraphMatrices::Item);
    }
    void Compute()
    {
        PROF_EVENT("Render.Graph.Compute");
        if (cancelled.load(std::memory_order_relaxed)) return;
        // Index sort preserves the association with each copied world transform.
        std::vector<u32> order(input.size());
        for (u32 i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](u32 ai, u32 bi)
        {
            const auto& a = input[ai];
            const auto& b = input[bi];
            if (!(a.sortKey == b.sortKey)) return a.sortKey < b.sortKey;
            if (a.static_batch != b.static_batch) return std::less<const void*>()(a.static_batch, b.static_batch);
            return a.static_first_index < b.static_first_index;
        });
        output.clear();
        for (u32 index : order)
        {
            if (cancelled.load(std::memory_order_relaxed)) return;
            output.push_back(input[index]);
            if (!dynamic) continue;
            auto& matrix = matrices[output.size() - 1];
            matrix.world = worlds[index];
            matrix.view.mul_43(view, matrix.world);
            matrix.projection.mul(projection, matrix.view);
            matrix.valid = _valid(matrix.world) && _valid(matrix.view) && _valid(matrix.projection);
        }
        ready.store(true, std::memory_order_release);
    }
};
}

struct RenderViewPreparation::State
{
    std::shared_ptr<SceneInput> scene;
    std::vector<std::shared_ptr<ViewJob>> views;
    std::vector<std::shared_ptr<GraphJob>> graphs;
    concurrency::task_group tasks;
    size_t graph_bytes = 0;
    u32 consumed_views = 0, consumed_graphs = 0, missed = 0;
    bool valid_scene = false, pending = false;
};

RenderViewPreparation::RenderViewPreparation() : state(new State()) {}
RenderViewPreparation::~RenderViewPreparation() { Clear(); }

void RenderViewPreparation::FinishFrame()
{
    if (!state->pending) return;
    PROF_EVENT("Render.Preparation.FinishFrame");
    // Fallback results are no longer useful; jobs only read their own storage.
    for (auto& graph : state->graphs) graph->cancelled.store(true, std::memory_order_relaxed);
    state->tasks.wait();
    state->pending = false;
    PROF_TAG("Queued views", u32(state->views.size()));
    PROF_TAG("Used views", state->consumed_views);
    PROF_TAG("Queued graphs", u32(state->graphs.size()));
    PROF_TAG("Used graphs", state->consumed_graphs);
    PROF_TAG("Missed results", state->missed);
}

void RenderViewPreparation::BeginFrame()
{
    FinishFrame();
    state->views.clear();
    state->graphs.clear();
    state->graph_bytes = 0;
    state->consumed_views = state->consumed_graphs = state->missed = 0;
    state->valid_scene = state->scene && ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC) &&
        state->scene->nodes.size() >= 256 && !Device.m_SecondViewport.IsSVPFrame();
    if (!state->valid_scene) return;
    PROF_EVENT("Render.Preparation.ValidateScene");
    // This check is once per owner frame, never per view or inside jobs. Mutable
    // hierarchy/bounds do not keep a stale load snapshot, including in-place edits.
    for (const auto& node : state->scene->nodes)
    {
        if (node.owner->Type != node.type || memcmp(&node.owner->vis.box, &node.bounds, sizeof(Fbox)) ||
            memcmp(&node.owner->vis.sphere, &node.sphere, sizeof(Fsphere)))
        { state->valid_scene = false; break; }
        if (node.kind != PreparedStaticCommand::Hierarchy) continue;
        const auto& hierarchy = *static_cast<FHierrarhyVisual*>(node.owner);
        if (!hierarchy.bDontDelete) { state->valid_scene = false; break; }
        const auto& children = hierarchy.children;
        if (children.size() != node.children.size()) { state->valid_scene = false; break; }
        for (u32 i = 0; i < children.size(); ++i)
            if (children[i] != state->scene->nodes[node.children[i]].owner) { state->valid_scene = false; break; }
        if (!state->valid_scene) break;
    }
}

void RenderViewPreparation::Clear()
{
    FinishFrame();
    state->views.clear();
    state->graphs.clear();
    state->scene.reset();
    state->valid_scene = false;
}

void RenderViewPreparation::Load(CRender& renderer)
{
    Clear();
    PROF_EVENT("Render.Preparation.LoadSnapshot");
    auto scene = std::make_shared<SceneInput>();
    std::unordered_map<dxRender_Visual*, u32> nodes;
    std::unordered_map<CPortal*, u32> portals;
    for (u32 i = 0; i < renderer.Sectors.size(); ++i)
        scene->sector_ids[static_cast<CSector*>(renderer.Sectors[i])] = i;
    for (u32 i = 0; i < renderer.Portals.size(); ++i)
    {
        auto* portal = static_cast<CPortal*>(renderer.Portals[i]);
        portals[portal] = i;
        auto front = scene->sector_ids.find(portal->Front()), back = scene->sector_ids.find(portal->Back());
        scene->portals.push_back({portal, portal->P, portal->S, portal->getPoly(),
            front == scene->sector_ids.end() ? no_index : front->second,
            back == scene->sector_ids.end() ? no_index : back->second});
    }
    auto capture = [&](auto&& self, dxRender_Visual* visual, u32 depth) -> u32
    {
        if (!visual || scene->nodes.size() >= max_nodes) return no_index;
        auto known = nodes.find(visual);
        if (known != nodes.end()) return known->second;
        const u32 id = u32(scene->nodes.size());
        nodes[visual] = id;
        scene->nodes.emplace_back();
        auto& node = scene->nodes[id];
        node.owner = visual;
        node.type = visual->Type;
        node.bounds = visual->vis.box;
        node.sphere = visual->vis.sphere;
        if (depth >= 64 || !node.bounds.is_valid() || !_valid(node.bounds.min) || !_valid(node.bounds.max) ||
            !_valid(node.sphere.P) || !_valid(node.sphere.R) || node.sphere.R < 0.f) return id;
        if (visual->Type == MT_TREE_ST || visual->Type == MT_TREE_PM)
        {
            node.tree = true;
            node.transform = static_cast<FTreeVisual*>(visual)->PreparationTransform();
        }
        if ((visual->Type == MT_NORMAL && static_cast<Fvisual*>(visual)->immutable_level_geometry) ||
            visual->Type == MT_TREE_ST || visual->Type == MT_TREE_PM || visual->Type == MT_PROGRESSIVE)
            node.kind = PreparedStaticCommand::Leaf;
        else if (visual->Type == MT_HIERRARHY)
        {
            auto* hierarchy = static_cast<FHierrarhyVisual*>(visual);
            if (!hierarchy->bDontDelete) return id;
            std::vector<u32> children;
            for (auto* child : hierarchy->children) children.push_back(self(self, static_cast<dxRender_Visual*>(child), depth + 1));
            if (std::find(children.begin(), children.end(), no_index) != children.end()) return id;
            scene->nodes[id].children = std::move(children);
            scene->nodes[id].kind = PreparedStaticCommand::Hierarchy;
        }
        return id;
    };
    for (auto* item : renderer.Sectors)
    {
        auto* sector = static_cast<CSector*>(item);
        SectorInput input{sector, capture(capture, sector->root(), 0), {}};
        for (auto* portal : sector->PreparationPortals()) input.portals.push_back(portals.at(portal));
        scene->sectors.push_back(std::move(input));
    }
    state->scene = std::move(scene);
}

void RenderViewPreparation::Queue(CSector* start, const CFrustum& view, const Fmatrix& transform,
    const Fvector& origin, bool camera, const Fmatrix& render_view)
{
    if (!state->valid_scene || !ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC) || !start ||
        state->scene->nodes.size() < 256 || state->views.size() >= 32 || Device.m_SecondViewport.IsSVPFrame()) return;
    const auto found = state->scene->sector_ids.find(start);
    if (found == state->scene->sector_ids.end()) return;
    auto job = std::make_shared<ViewJob>();
    job->start = found->second;
    job->view = view;
    job->transform = transform;
    job->origin = origin;
    job->camera = camera;
    job->render_view = render_view;
    for (const auto& portal : state->scene->portals) job->dual.push_back(!!portal.owner->bDualRender);
    state->views.push_back(job);
    state->pending = true;
    auto scene = state->scene;
    const DWORD owner = GetCurrentThreadId();
    state->tasks.run([job, scene, owner]()
    {
        PROF_THREAD_IF(GetCurrentThreadId() != owner, "Render preparation worker");
        job->Compute(*scene);
    });
}

bool RenderViewPreparation::RenderStatic(CDSGraphManager& graph, dxRender_Visual* root, const CFrustum& view, bool camera)
{
    if (!state->valid_scene || !ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC) || Device.m_SecondViewport.IsSVPFrame()) return false;
    for (const auto& item : state->views)
    {
        const auto& job = *item;
        if (!job.ready.load(std::memory_order_acquire) || !job.valid || job.camera != camera) continue;
        for (const auto& output : job.outputs)
        {
            const u32 root_id = state->scene->sectors[output.sector].root;
            if (root_id == no_index || state->scene->nodes[root_id].owner != root || view.p_count != output.view.p_count ||
                memcmp(view.planes, output.view.planes, view.p_count * sizeof(CFrustum::fplane))) continue;
            PROF_EVENT("Render.Views.Consume");
            for (u32 i = 0; i < output.commands.size();)
            {
                const auto& command = output.commands[i];
                if (!graph.ApplyPreparedStatic(command, view)) { i = command.end; continue; }
                if (command.tree != no_index)
                {
                    const auto& tree = job.trees[command.tree];
                    static_cast<FTreeVisual*>(command.visual)->PublishPreparedView(job.render_view, tree.world, tree.combined);
                }
                ++i;
            }
            ++state->consumed_views;
            return true;
        }
    }
    ++state->missed;
    return false;
}

void RenderViewPreparation::QueueGraph(CDSGraphManager& graph, const Fmatrix& view, const Fmatrix& projection,
    bool static_only, bool dynamic_only)
{
    if (!ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC) || !_valid(view) || !_valid(projection)) return;
    PROF_EVENT("Render.Graph.Snapshot");
    auto capture = [&](R_dsgraph::RenderQueue& queue, bool dynamic)
    {
        if (queue.size() < 32 || queue.size() > 65536 || state->graphs.size() >= 128) return;
        for (const auto& old : state->graphs)
            if (old->target == &queue && old->phase == RImplementation.phase && old->input.size() == queue.size()) return;
        const size_t bytes = queue.size() * (2 * sizeof(R_dsgraph::RenderPacket) +
            (dynamic ? sizeof(Fmatrix) + sizeof(PreparedGraphMatrices::Item) : 0));
        if (bytes > graph_memory_limit - state->graph_bytes) return;
        auto job = std::make_shared<GraphJob>();
        job->target = &queue;
        job->dynamic = dynamic;
        job->phase = RImplementation.phase;
        job->view = view;
        job->projection = projection;
        job->input = queue;
        job->output.reserve(queue.size());
        if (dynamic)
        {
            job->worlds.reserve(queue.size());
            job->matrices.resize(queue.size());
            for (const auto& packet : queue)
            {
                if (!packet.item.pMatrix || !_valid(*packet.item.pMatrix)) return;
                job->worlds.push_back(*packet.item.pMatrix);
            }
        }
        state->graph_bytes += job->Bytes();
        if (state->graph_bytes > graph_memory_limit) { state->graph_bytes -= job->Bytes(); return; }
        state->graphs.push_back(job);
        state->pending = true;
        const DWORD owner = GetCurrentThreadId();
        state->tasks.run([job, owner]()
        {
            PROF_THREAD_IF(GetCurrentThreadId() != owner, "Render preparation worker");
            job->Compute();
        });
    };
    for (u32 priority = 0; priority < 2; ++priority)
        for (u32 pass = 0; pass < SHADER_PASSES_MAX; ++pass)
        {
            if (!dynamic_only) capture(graph.RGraph.mapStaticPasses[priority][pass], false);
            if (!static_only) capture(graph.RGraph.mapDynamicPasses[priority][pass], true);
        }
}

const PreparedGraphMatrices* RenderViewPreparation::Consume(R_dsgraph::RenderQueue& queue, bool dynamic)
{
    for (const auto& item : state->graphs)
    {
        auto& job = *item;
        if (job.target != &queue || job.dynamic != dynamic || job.phase != RImplementation.phase) continue;
        if (!ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC) || !job.ready.load(std::memory_order_acquire))
        { job.cancelled.store(true, std::memory_order_relaxed); ++state->missed; return nullptr; }
        if (queue.size() != job.input.size()) return nullptr;
        if (dynamic && (memcmp(&job.view, &RCache.get_xform_view(), sizeof(Fmatrix)) ||
            memcmp(&job.projection, &RCache.get_xform_project(), sizeof(Fmatrix)))) return nullptr;
        for (u32 i = 0; i < queue.size(); ++i)
            if (!SamePacket(queue[i], job.input[i]) || (dynamic && memcmp(queue[i].item.pMatrix, &job.worlds[i], sizeof(Fmatrix)))) return nullptr;
        PROF_EVENT("Render.Graph.Consume");
        queue.swap(job.output);
        job.target = nullptr; // A later invocation cannot reuse a sorted or changed source.
        ++state->consumed_graphs;
        return &job;
    }
    return nullptr;
}
