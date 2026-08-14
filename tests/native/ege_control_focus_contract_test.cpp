#include "test_support.h"
#include "ege_head.h"
#include "ege_graph.h"
#include "ege/egecontrolbase.h"

namespace ege
{
namespace
{

void testDetachingSubtreeClearsDescendantFocus()
{
    EGE_CHECK(graph_setting.egectrl_root == nullptr);
    EGE_CHECK(graph_setting.egectrl_focus == nullptr);

    egeControlBase root;
    egeControlBase child;
    egeControlBase grandchild;
    EGE_CHECK(graph_setting.egectrl_root == &root);
    child.addchild(&grandchild);

    EGE_CHECK(child.parent() == &root);
    EGE_CHECK(grandchild.parent() == &child);
    graph_setting.egectrl_focus = &grandchild;

    EGE_CHECK(root.delchild(&child) == 1);
    EGE_CHECK(child.parent() == nullptr);
    EGE_CHECK(grandchild.parent() == &child);
    EGE_CHECK(graph_setting.egectrl_focus == nullptr);
}

void testReparentingPreservesReachableDescendantFocus()
{
    egeControlBase root;
    egeControlBase firstParent;
    egeControlBase secondParent;
    egeControlBase child;
    firstParent.addchild(&child);
    graph_setting.egectrl_focus = &child;

    secondParent.addchild(&firstParent);
    EGE_CHECK(firstParent.parent() == &secondParent);
    EGE_CHECK(child.parent() == &firstParent);
    EGE_CHECK(graph_setting.egectrl_focus == &child);

    graph_setting.egectrl_focus = nullptr;
}

void testDestroyingIntermediateControlPreservesReparentedFocus()
{
    egeControlBase root;
    egeControlBase child;
    egeControlBase* parent = new egeControlBase;
    parent->addchild(&child);
    graph_setting.egectrl_focus = &child;

    delete parent;
    EGE_CHECK(child.parent() == &root);
    EGE_CHECK(graph_setting.egectrl_focus == &child);

    graph_setting.egectrl_focus = nullptr;
}

} // namespace
} // namespace ege

int main()
{
    ege::testDetachingSubtreeClearsDescendantFocus();
    ege::testReparentingPreservesReachableDescendantFocus();
    ege::testDestroyingIntermediateControlPreservesReparentedFocus();
    EGE_CHECK(ege::graph_setting.egectrl_root == nullptr);
    EGE_CHECK(ege::graph_setting.egectrl_focus == nullptr);
    return ege_test::finish("EGE control focus contract");
}
