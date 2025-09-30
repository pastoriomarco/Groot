#ifndef NODE_UTILS_H
#define NODE_UTILS_H

#include <QDomDocument>
#include <set>
#include <nodes/NodeData>
#include <nodes/FlowScene>
#include <nodes/NodeStyle>

#include "bt_editor_base.h"
#include <behaviortree_cpp_v3/flatbuffers/BT_logger_generated.h>
#include <behaviortree_cpp_v3/flatbuffers/bt_flatbuffer_helper.h>

QtNodes::Node* findRoot(const QtNodes::FlowScene &scene);

std::vector<QtNodes::Node *> getChildren(const QtNodes::FlowScene &scene,
                                         const QtNodes::Node &parent_node,
                                         bool ordered);

AbsBehaviorTree BuildTreeFromScene(const QtNodes::FlowScene *scene,
                                   QtNodes::Node *root_node = nullptr);

std::pair<AbsBehaviorTree, std::unordered_map<int, int> >
BuildTreeFromFlatbuffers(const Serialization::BehaviorTree* bt );

AbsBehaviorTree BuildTreeFromXML(const QDomElement &bt_root, const NodeModels &models);

void NodeReorder(QtNodes::FlowScene &scene, AbsBehaviorTree &abstract_tree );

std::pair<QtNodes::NodeStyle, QtNodes::ConnectionStyle>
getStyleFromStatus(NodeStatus status, NodeStatus prev_status);

QtNodes::Node* GetParentNode(QtNodes::Node* node);

std::set<QString> GetModelsToRemove(QWidget* parent,
                                    NodeModels& prev_models,
                                    const NodeModels& new_models);

BT::NodeType convert( Serialization::NodeType type);

BT::NodeStatus convert(Serialization::NodeStatus type);

BT::PortDirection convert(Serialization::PortDirection direction);

// Returns the set of nodes in the subtree rooted at root_node (excluding root by default).
// If include_root is true, the root node is also included.
std::set<QtNodes::Node*> GetSubtreeNodesRecursively(const QtNodes::FlowScene &scene,
                                                    QtNodes::Node &root_node,
                                                    bool include_root = false);

// Set visible flag for all nodes in the subtree (excluding root unless include_root is true)
// and for all connections attached to those nodes.
void SetSubtreeVisible(QtNodes::FlowScene &scene,
                       QtNodes::Node &root_node,
                       bool visible,
                       bool include_root = false);

// Fetch per-model caption color defined in resources/NodesStyle.json.
// If no specific color is found, returns defaultColor.
QColor GetCaptionColorForModel(const NodeModel &model,
                               const QColor &defaultColor = QColor("#ffffff"));

// Recalculate geometry and move connections for all nodes + connections in the scene.
// Does not change positions or collapsed state; only refreshes visuals.
void RefreshSceneGraphics(QtNodes::FlowScene &scene);

// Restore visibility of a subtree when expanding a node, while respecting the
// collapsed state of descendant nodes (i.e., keep their subtrees hidden).
void RestoreVisibilityRespectingCollapsed(QtNodes::FlowScene &scene,
                                          QtNodes::Node &root_node);


#endif // NODE_UTILS_H
