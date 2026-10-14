#pragma once
#include <memory>
#include <vector>

class CRender;
class CDSGraphManager;
class CSector;
class dxRender_Visual;

struct PreparedStaticCommand
{
    enum Kind { Hierarchy, Leaf, Fallback } kind = Fallback;
    dxRender_Visual* visual = nullptr; // Identity only on workers.
    u32 end = 0, planes = 0;
    bool full = false;
    u32 tree = u32(-1);
};

struct PreparedGraphMatrices
{
    Fmatrix view, projection;
    struct Item
    {
        Fmatrix world, view, projection;
        bool valid = false;
    };
    std::vector<Item> matrices;
};

// Copied same-frame inputs only. The owner consumes ready results, falls back
// without waiting and joins before frame inputs or level resources can retire.
class RenderViewPreparation
{
    struct State;
    std::unique_ptr<State> state;
public:
    RenderViewPreparation();
    ~RenderViewPreparation();
    void Load(CRender& renderer);
    void Clear();
    void BeginFrame();
    void FinishFrame();
    void Queue(CSector* start, const CFrustum& view, const Fmatrix& transform,
        const Fvector& origin, bool camera, const Fmatrix& render_view);
    bool RenderStatic(CDSGraphManager& graph, dxRender_Visual* root, const CFrustum& view, bool camera);
    void QueueGraph(CDSGraphManager& graph, const Fmatrix& view, const Fmatrix& projection,
        bool static_only = false, bool dynamic_only = false);
    const PreparedGraphMatrices* Consume(R_dsgraph::RenderQueue& queue, bool dynamic);
};
