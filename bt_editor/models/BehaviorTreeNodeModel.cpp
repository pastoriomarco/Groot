#include "BehaviorTreeNodeModel.hpp"
#include <QBoxLayout>
#include <QFormLayout>
#include <QSizePolicy>
#include <QLineEdit>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QApplication>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QTimer>
#include <nodes/FlowScene>
#include <nodes/internal/NodeGraphicsObject.hpp>
#include <QGraphicsProxyWidget>
#include <QGraphicsItem>
#include <QGraphicsDropShadowEffect>
#include <climits>

const int MARGIN = 10;
const int DEFAULT_LINE_WIDTH  = 100;
const int DEFAULT_FIELD_WIDTH = 50;
const int DEFAULT_LABEL_WIDTH = 50;

BehaviorTreeDataModel::BehaviorTreeDataModel(const NodeModel &model):
    _params_widget(nullptr),
    _uid( GetUID() ),
    _model(model),
    _icon_renderer(nullptr),
    _style_caption_color( QtNodes::NodeStyle().FontColor ),
    _style_caption_alias( model.registration_ID )
{
    readStyle();
    _main_widget = new QFrame();
    _line_edit_name = new QLineEdit(_main_widget);
    _params_widget = new QFrame();

    _main_layout = new QVBoxLayout(_main_widget);
    _main_widget->setLayout( _main_layout );

    auto capt_layout = new QHBoxLayout();

    _caption_label = new QLabel();
    _caption_logo_left  = new QFrame();
    _caption_logo_right = new QFrame();
    _caption_logo_left->setFixedSize( QSize(0,20) );
    _caption_logo_right->setFixedSize( QSize(0,20) );
    _caption_label->setFixedHeight(20);

    _caption_logo_left->installEventFilter(this);
    _caption_logo_right->installEventFilter(this);
    _caption_label->installEventFilter(this);
    _main_widget->installEventFilter(this);

    QFont capt_font = _caption_label->font();
    capt_font.setPointSize(12);
    _caption_label->setFont(capt_font);

    capt_layout->addWidget(_caption_logo_left, 0, Qt::AlignRight);
    capt_layout->addWidget(_caption_label, 0, Qt::AlignHCenter );
    capt_layout->addWidget(_caption_logo_right, 0, Qt::AlignLeft);

    _main_layout->addLayout( capt_layout );
    _main_layout->addWidget( _line_edit_name );

    _main_layout->setMargin(0);
    _main_layout->setSpacing(2);

    //----------------------------
    _line_edit_name->setAlignment( Qt::AlignCenter );
    _line_edit_name->setText( _instance_name );
    _line_edit_name->setFixedWidth( DEFAULT_LINE_WIDTH );

    _main_widget->setAttribute(Qt::WA_NoSystemBackground);

    _line_edit_name->setStyleSheet("color: white; "
                                   "background-color: transparent;"
                                   "border: 0px;");

    //--------------------------------------
    _form_layout = new QFormLayout( _params_widget );
    _form_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    _main_layout->addWidget(_params_widget);
    // Inline container for collapsed children (hidden by default)
    _inline_container = new QFrame();
    _inline_layout = new QVBoxLayout(_inline_container);
    _inline_layout->setContentsMargins(6,4,6,10);
    _inline_layout->setSpacing(4);
    _inline_container->setVisible(false);
    _main_layout->addWidget(_inline_container);
    _params_widget->setStyleSheet("color: white;");

    _form_layout->setHorizontalSpacing(4);
    _form_layout->setVerticalSpacing(2);
    _form_layout->setContentsMargins(0, 0, 0, 0);

    PortDirection preferred_port_types[3] = { PortDirection::INPUT,
                                              PortDirection::OUTPUT,
                                              PortDirection::INOUT};

    for(int pref_index=0; pref_index < 3; pref_index++)
    {
        for(const auto& port_it: model.ports )
        {
            auto preferred_direction = preferred_port_types[pref_index];
            if( port_it.second.direction != preferred_direction )
            {
                continue;
            }

            QString description = port_it.second.description;
            QString label = port_it.first;
            if( preferred_direction == PortDirection::INPUT)
            {
                label.prepend("[IN] ");
                if( description.isEmpty())
                {
                    description="[INPUT]";
                }
                else{
                    description.prepend("[INPUT]: ");
                }
            }
            else if( preferred_direction == PortDirection::OUTPUT){
                label.prepend("[OUT] ");
                if( description.isEmpty())
                {
                    description="[OUTPUT]";
                }
                else{
                    description.prepend("[OUTPUT]: ");
                }
            }

            GrootLineEdit* form_field = new GrootLineEdit();
            form_field->setAlignment( Qt::AlignHCenter);
            form_field->setMaximumWidth(140);
            form_field->setText( port_it.second.default_value );

            connect(form_field, &GrootLineEdit::doubleClicked,
                    this, [this,form_field]()
                    { emit this->portValueDoubleChicked(form_field); });

            connect(form_field, &GrootLineEdit::lostFocus,
                    this, [this]()
                    { emit this->portValueDoubleChicked(nullptr); });

            QLabel* form_label  =  new QLabel( label, _params_widget );
            form_label->setStyleSheet("QToolTip {color: black;}");
            form_label->setToolTip( description );

            form_field->setMinimumWidth(DEFAULT_FIELD_WIDTH);

            _ports_widgets.insert( std::make_pair( port_it.first, form_field) );

            form_field->setStyleSheet("color: rgb(30,30,30); "
                                      "background-color: rgb(200,200,200); "
                                      "border: 0px; ");

            _form_layout->addRow( form_label, form_field );

            auto paramUpdated = [this,label,form_field]()
            {
                this->parameterUpdated(label,form_field);
            };

            if(auto lineedit = dynamic_cast<QLineEdit*>( form_field ) )
            {
                connect( lineedit, &QLineEdit::editingFinished, this, paramUpdated );
                connect( lineedit, &QLineEdit::editingFinished,
                         this, &BehaviorTreeDataModel::updateNodeSize);
            }
            else if( auto combo = dynamic_cast<QComboBox*>( form_field ) )
            {
                connect( combo, &QComboBox::currentTextChanged, this, paramUpdated);
            }
        }
    }
    _params_widget->adjustSize();

    capt_layout->setSizeConstraint(QLayout::SizeConstraint::SetMaximumSize);
    _main_layout->setSizeConstraint(QLayout::SizeConstraint::SetMaximumSize);
    _form_layout->setSizeConstraint(QLayout::SizeConstraint::SetMaximumSize);
    //--------------------------------------
    connect( _line_edit_name, &QLineEdit::editingFinished,
             this, [this]()
    {
        setInstanceName( _line_edit_name->text() );
    });

    // Prepare UI affordance for collapse toggle (only for Sequence-like nodes)
    connectCollapseToggleUI();
}

BehaviorTreeDataModel::~BehaviorTreeDataModel()
{

}

BT::NodeType BehaviorTreeDataModel::nodeType() const
{
    return _model.type;
}

void BehaviorTreeDataModel::initWidget()
{
    if( _style_icon.isEmpty() == false )
    {
        _caption_logo_left->setFixedWidth( 20 );
        _caption_logo_right->setFixedWidth( 1 );

        QFile file(_style_icon);
        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug()<<"file not opened: "<< _style_icon;
            file.close();
        }
        else {
            QByteArray ba = file.readAll();
            QByteArray new_color_fill = QString("fill:%1;").arg( _style_caption_color.name() ).toUtf8();
            ba.replace("fill:#ffffff;", new_color_fill);
            _icon_renderer =  new QSvgRenderer(ba, this);
        }
    }

    _caption_label->setText( _style_caption_alias );

    QPalette capt_palette = _caption_label->palette();
    capt_palette.setColor(_caption_label->backgroundRole(), Qt::transparent);
    capt_palette.setColor(_caption_label->foregroundRole(), _style_caption_color);
    _caption_label->setPalette(capt_palette);

    _caption_logo_left->adjustSize();
    _caption_logo_right->adjustSize();
    _caption_label->adjustSize();

    updateNodeSize();
    // Ensure collapse UI hint (width/cursor) is applied after initWidget adjustments
    connectCollapseToggleUI();
}

unsigned int BehaviorTreeDataModel::nPorts(QtNodes::PortType portType) const
{
    if( portType == QtNodes::PortType::Out)
    {
        if( nodeType() == NodeType::ACTION || nodeType() == NodeType::CONDITION )
        {
            return 0;
        }
        else{
            return 1;
        }
    }
    else if( portType == QtNodes::PortType::In )
    {
        return (_model.registration_ID == "Root") ? 0 : 1;
    }
    return 0;
}

NodeDataModel::ConnectionPolicy BehaviorTreeDataModel::portOutConnectionPolicy(QtNodes::PortIndex) const
{
    return ( nodeType() == NodeType::DECORATOR || _model.registration_ID == "Root") ? ConnectionPolicy::One : ConnectionPolicy::Many;
}

void BehaviorTreeDataModel::updateNodeSize()
{
    int caption_width = _caption_label->width();
    caption_width += _caption_logo_left->width() + _caption_logo_right->width();
    int line_edit_width =  caption_width;

    if( _line_edit_name->isHidden() == false)
    {
        QFontMetrics fm = _line_edit_name->fontMetrics();
        const QString& txt = _line_edit_name->text();
        int text_width = fm.boundingRect(txt).width();
        line_edit_width = std::max( line_edit_width, text_width + MARGIN);
    }

    //----------------------------
    int field_colum_width = DEFAULT_LABEL_WIDTH;
    int label_colum_width = 0;

    for(int row = 0; row< _form_layout->rowCount(); row++)
    {
        auto label_widget = _form_layout->itemAt(row, QFormLayout::LabelRole)->widget();
        auto field_widget = _form_layout->itemAt(row, QFormLayout::FieldRole)->widget();
        if(auto field_line_edit = dynamic_cast<QLineEdit*>(field_widget))
        {
            QFontMetrics fontMetrics = field_line_edit->fontMetrics();
            QString text = field_line_edit->text();
            int text_width = fontMetrics.boundingRect(text).width();
            field_colum_width = std::max( field_colum_width, text_width + MARGIN);
        }
        label_colum_width = std::max(label_colum_width, label_widget->width());
    }

    field_colum_width = std::max( field_colum_width,
                                  line_edit_width - label_colum_width - _form_layout->spacing());

    for(int row = 0; row< _form_layout->rowCount(); row++)
    {
        auto field_widget = _form_layout->itemAt(row, QFormLayout::FieldRole)->widget();
        if(auto field_line_edit = dynamic_cast<QLineEdit*>(field_widget))
        {
            field_line_edit->setFixedWidth( field_colum_width );
        }
    }
    line_edit_width = std::max( line_edit_width, label_colum_width + field_colum_width );
    _line_edit_name->setFixedWidth( line_edit_width);

    _params_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    _params_widget->adjustSize();

    _main_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    _main_widget->adjustSize();

    //----------------------------

    emit embeddedWidgetSizeUpdated();
}

QtNodes::NodeDataType BehaviorTreeDataModel::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return NodeDataType {"", ""};
}

std::shared_ptr<QtNodes::NodeData> BehaviorTreeDataModel::outData(QtNodes::PortIndex)
{
    return nullptr;
}

void BehaviorTreeDataModel::readStyle()
{
    QFile style_file(":/NodesStyle.json");

    if (!style_file.open(QIODevice::ReadOnly))
    {
        qWarning("Couldn't open NodesStyle.json");
        return;
    }

    QByteArray bytearray =  style_file.readAll();
    style_file.close();
    QJsonParseError error;
    QJsonDocument json_doc( QJsonDocument::fromJson( bytearray, &error ));

    if(json_doc.isNull()){
        qDebug()<<"Failed to create JSON doc: " << error.errorString();
        return;
    }
    if(!json_doc.isObject()){
        qDebug()<<"JSON is not an object.";
        return;
    }

    QJsonObject toplevel_object = json_doc.object();

    if(toplevel_object.isEmpty()){
        qDebug()<<"JSON object is empty.";
        return;
    }
    QString model_type_name( QString::fromStdString(toStr(_model.type)) );

    for (const auto& model_name: { model_type_name, _model.registration_ID} )
    {
        if( toplevel_object.contains(model_name) )
        {
            auto category_style = toplevel_object[ model_name ].toObject() ;
            if( category_style.contains("icon"))
            {
                _style_icon = category_style["icon"].toString();
            }
            if( category_style.contains("caption_color"))
            {
                _style_caption_color = category_style["caption_color"].toString();
            }
            if( category_style.contains("caption_alias"))
            {
                _style_caption_alias = category_style["caption_alias"].toString();
            }
        }
    }
}

const QString& BehaviorTreeDataModel::registrationName() const
{
    return _model.registration_ID;
}

const QString &BehaviorTreeDataModel::instanceName() const
{
    return _instance_name;
}

PortsMapping BehaviorTreeDataModel::getCurrentPortMapping() const
{
    PortsMapping out;

    for(const auto& it: _ports_widgets)
    {
        const auto& label = it.first;
        const auto& value = it.second;

        if(auto linedit = dynamic_cast<QLineEdit*>( value ) )
        {
            out.insert( std::make_pair( label, linedit->text() ) );
        }
        else if( auto combo = dynamic_cast<QComboBox*>( value ) )
        {
            out.insert( std::make_pair( label, combo->currentText() ) );
        }
    }
    return out;
}

QJsonObject BehaviorTreeDataModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"]  = registrationName();
    modelJson["alias"] = instanceName();

    for (const auto& it: _ports_widgets)
    {
        if( auto linedit = dynamic_cast<QLineEdit*>(it.second)){
            modelJson[it.first] = linedit->text();
        }
        else if( auto combo = dynamic_cast<QComboBox*>(it.second)){
            modelJson[it.first] = combo->currentText();
        }
    }

    // Persist collapsed state for all nodes
    modelJson["collapsed"] = _collapsed;
    modelJson["collapse_nested"] = _collapse_nested;

    return modelJson;
}

void BehaviorTreeDataModel::restore(const QJsonObject &modelJson)
{
    if( registrationName() != modelJson["name"].toString() )
    {
        throw std::runtime_error(" error restoring: different registration_name");
    }
    QString alias = modelJson["alias"].toString();
    setInstanceName( alias );

    for(auto it = modelJson.begin(); it != modelJson.end(); it++ )
    {
        if( it.key() != "alias" && it.key() != "name")
        {
            if (it.key() == "collapsed")
            {
                _collapsed = it.value().toBool(false);
            }
            else if (it.key() == "collapse_nested")
            {
                _collapse_nested = it.value().toBool(true);
            }
            else
            {
                setPortMapping( it.key(), it.value().toString() );
            }
        }
    }

    if (_collapsed)
    {
        // Defer until scene has restored connections
        QTimer::singleShot(0, [this]() { setCollapsed(true); });
    }

}

void BehaviorTreeDataModel::lock(bool locked)
{
    _line_edit_name->setEnabled( !locked );

    for(const auto& it: _ports_widgets)
    {
        const auto& field_widget = it.second;

        if(auto lineedit = dynamic_cast<QLineEdit*>( field_widget  ))
        {
            lineedit->setReadOnly( locked );
        }
        else if(auto combo = dynamic_cast<QComboBox*>( field_widget ))
        {
            combo->setEnabled( !locked );
        }
    }
}

void BehaviorTreeDataModel::setPortMapping(const QString &port_name, const QString &value)
{
    auto it = _ports_widgets.find(port_name);
    if( it != _ports_widgets.end() )
    {
        if( auto lineedit = dynamic_cast<QLineEdit*>(it->second) )
        {
            lineedit->setText(value);
        }
        else if( auto combo = dynamic_cast<QComboBox*>(it->second) )
        {
            int index = combo->findText(value);
            if( index == -1 ){
                qDebug() << "error, combo value "<< value << " not found";
            }
            else{
                combo->setCurrentIndex(index);
            }
        }
    }
    else{
        qDebug() << "error, label "<< port_name << " not found in the model";
    }
}

bool BehaviorTreeDataModel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Paint && obj == _caption_logo_left && _icon_renderer)
    {
        QPainter paint(_caption_logo_left);
        _icon_renderer->render(&paint);
    }
    // Toggle/cycle collapse: Middle-click (monitor/locked cycles) or Left-click on chevron (editor toggles)
    // Determine if node is locked (Monitor mode typically locks nodes)
    bool node_locked = false;
    if (_main_widget)
    {
        if (auto proxy = _main_widget->graphicsProxyWidget())
        {
            if (auto ngo = dynamic_cast<QtNodes::NodeGraphicsObject*>(proxy->parentItem()))
            {
                node_locked = !(ngo->flags() & QGraphicsItem::ItemIsMovable);
            }
        }
    }

    // Middle-click press cycles and consumes the event (when locked/monitor to avoid conflicting with panning).
    if (event->type() == QEvent::MouseButtonPress &&
        (obj == _caption_logo_right || obj == _caption_label || obj == _main_widget))
    {
        auto me = static_cast<QMouseEvent*>(event);
        if (node_locked && me->button() == Qt::MiddleButton)
        {
            cycleCollapseMode();
            return true; // stop context menus and scene panning
        }
    }
    // Left-click release toggles only when clicking the explicit right header area (editor/unlocked).
    if (event->type() == QEvent::MouseButtonRelease && obj == _caption_logo_right)
    {
        auto me = static_cast<QMouseEvent*>(event);
        if (!node_locked && me->button() == Qt::LeftButton)
        {
            if (!_collapsed)
            {
                _collapse_nested = true; // default editor collapse shows nested
                setCollapsed(true);
            }
            else
            {
                setCollapsed(false);
            }
            return true;
        }
    }
    return NodeDataModel::eventFilter(obj, event);
}


void BehaviorTreeDataModel::setInstanceName(const QString &name)
{
    _instance_name = name;
    _line_edit_name->setText( name );

    updateNodeSize();
    emit instanceNameChanged();
}


void BehaviorTreeDataModel::onHighlightPortValue(QString value)
{
    for( const auto& it:  _ports_widgets)
    {
        if( auto line_edit = dynamic_cast<QLineEdit*>(it.second) )
        {
            QString line_str = line_edit->text();
            if( !value.isEmpty() && line_str == value )
            {
                line_edit->setStyleSheet("color: rgb(30,30,30); "
                                         "background-color: #ffef0b; "
                                         "border: 0px; ");
            }
            else{
                line_edit->setStyleSheet("color: rgb(30,30,30); "
                                         "background-color: rgb(200,200,200); "
                                         "border: 0px; ");
            }
        }
    }
}

void GrootLineEdit::mouseDoubleClickEvent(QMouseEvent *ev)
{
    //QLineEdit::mouseDoubleClickEvent(ev);
    emit doubleClicked();
}

void GrootLineEdit::focusOutEvent(QFocusEvent *ev)
{
    QLineEdit::focusOutEvent(ev);
    emit lostFocus();
}

//------------------- Collapsed Sequence helpers -------------------

bool BehaviorTreeDataModel::isSequenceLike() const
{
    // Make every node collapsible; this selector now always returns true
    Q_UNUSED(_model);
    return true;
}

void BehaviorTreeDataModel::connectCollapseToggleUI()
{
    _caption_logo_right->setFixedWidth(16);
    _caption_logo_right->setToolTip("Toggle collapse (Left-click in editor, Middle-click in monitor)");
    _caption_logo_right->setCursor(Qt::PointingHandCursor);
}

void BehaviorTreeDataModel::rebuildInlineChildren()
{
    // Clear existing
    if (_inline_layout)
    {
        while (_inline_layout->count() > 0)
        {
            auto item = _inline_layout->takeAt(0);
            if (auto w = item->widget()) w->deleteLater();
            delete item;
        }
    }
    _inline_tokens.clear();

    auto proxy = _main_widget->graphicsProxyWidget();
    if (!proxy) return;
    auto parent_item = proxy->parentItem();
    auto ngo = dynamic_cast<QtNodes::NodeGraphicsObject*>(parent_item);
    if (!ngo) return;
    auto scene = dynamic_cast<QtNodes::FlowScene*>(ngo->scene());
    if (!scene) return;
    QtNodes::Node &this_node = ngo->node();

    auto ordered_children = getChildren(*scene, this_node, true);

    int maxDepth = _collapse_nested ? INT_MAX : 0;
    // Tighter outer layout for direct-children mode
    if (_inline_layout)
    {
        if (_collapse_nested)
        {
            _inline_layout->setContentsMargins(6,4,6,10);
            _inline_layout->setSpacing(4);
        }
        else
        {
            _inline_layout->setContentsMargins(6,2,6,6);
            _inline_layout->setSpacing(2);
        }
    }
    for (auto child : ordered_children)
    {
        auto token = buildInlineTokenForNode(child, scene, 0, maxDepth);
        if (token)
        {
            _inline_layout->addWidget(token);
        }
    }
    if (_inline_container->layout()) _inline_container->layout()->activate();
    _inline_container->adjustSize();
}

void BehaviorTreeDataModel::setCollapsed(bool collapsed)
{
    _collapsed = collapsed;

    auto proxy = _main_widget->graphicsProxyWidget();
    if (!proxy) return;
    auto parent_item = proxy->parentItem();
    auto ngo = dynamic_cast<QtNodes::NodeGraphicsObject*>(parent_item);
    if (!ngo) return;
    auto scene = dynamic_cast<QtNodes::FlowScene*>(ngo->scene());
    if (!scene) return;
    QtNodes::Node &this_node = ngo->node();

    if (_collapsed)
    {
        // Reset any previous hard constraints before rebuilding
        _inline_container->setMinimumSize(0, 0);
        _inline_container->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));

        rebuildInlineChildren();
        _inline_container->setVisible(true);
        if (_params_widget) _params_widget->setVisible(false);
        // Ensure layouts are activated and embedded widget grows to contain inline tokens
        if (_inline_container->layout()) _inline_container->layout()->activate();
        if (_main_widget->layout()) _main_widget->layout()->activate();
        _inline_container->adjustSize();
        // In direct-children view, ensure parent size equals stacked one-line tokens and widest token
        if (!_collapse_nested && _inline_layout)
        {
            QMargins m = _inline_layout->contentsMargins();
            int spacing = _inline_layout->spacing();
            int n = _inline_layout->count();
            int sum = m.top() + m.bottom();
            int maxW = 0;
            for (int i = 0; i < n; ++i)
            {
                if (auto item = _inline_layout->itemAt(i))
                {
                    if (auto w = item->widget())
                    {
                        QSize sh = w->sizeHint();
                        sum += sh.height();
                        maxW = std::max(maxW, sh.width());
                    }
                }
                if (i + 1 < n) sum += spacing;
            }
            // Fix container to the exact stacked size in compact mode
            int containerW = maxW + m.left() + m.right();
            _inline_container->setMinimumSize(containerW, sum);
            _inline_container->setMaximumHeight(sum);
        }
        // Allow shrink from previous larger collapsed mode
        _main_widget->setMinimumSize(0, 0);
        _main_widget->adjustSize();
        // Enforce a minimum based on inline content, then size exactly to it
        int minW = _inline_container->sizeHint().width() + 8;
        int minH = _inline_container->sizeHint().height() + 12;
        _main_widget->setMinimumSize(minW, minH);
        _main_widget->updateGeometry();
        _main_widget->resize(minW, minH);
        SetSubtreeVisible(*scene, this_node, /*visible*/false, /*include_root*/false);
        // Remove drop shadow effect while collapsed to avoid offscreen composition
        if (ngo->graphicsEffect())
        {
            ngo->setGraphicsEffect(nullptr);
        }
        if (auto proxy = _main_widget->graphicsProxyWidget())
        {
            // Clear previous constraints to allow shrink/grow between modes
            proxy->setMinimumSize(QSize(0,0));
            proxy->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
            proxy->update();
        }
    }
    else
    {
        _inline_container->setVisible(false);
        if (_params_widget) _params_widget->setVisible(true);
        // Clear enforced minimums when expanding back
        _inline_container->setMinimumSize(0, 0);
        _inline_container->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        _main_widget->setMinimumSize(0, 0);
        if (_main_widget->layout()) _main_widget->layout()->activate();
        _main_widget->adjustSize();
        _main_widget->resize(_main_widget->sizeHint());
        // Restore descendants visibility respecting any collapsed descendants
        RestoreVisibilityRespectingCollapsed(*scene, this_node);
        // Restore drop shadow effect
        if (!ngo->graphicsEffect())
        {
            auto effect = new QGraphicsDropShadowEffect;
            auto nodeStyle = ngo->node().nodeDataModel()->nodeStyle();
            effect->setOffset(2, 2);
            effect->setBlurRadius(5);
            effect->setColor(nodeStyle.ShadowColor);
            ngo->setGraphicsEffect(effect);
        }
        _inline_tokens.clear();
    }

    updateNodeSize();
    ngo->setGeometryChanged();
    ngo->update();
    ngo->moveConnections();

    // In Monitor mode (locked), reflow the entire tree so new node sizes are
    // factored into positions; in Editor, don't auto-reflow.
    bool node_locked = !(ngo->flags() & QGraphicsItem::ItemIsMovable);
    if (node_locked)
    {
        auto abs_tree = BuildTreeFromScene(scene);
        NodeReorder(*scene, abs_tree);
        RefreshSceneGraphics(*scene);
    }
}

QFrame* BehaviorTreeDataModel::buildInlineTokenForNode(QtNodes::Node* node,
                                                      QtNodes::FlowScene* scene,
                                                      int depth,
                                                      int maxDepth)
{
    if (!node || !scene) return nullptr;

    auto child_bt = dynamic_cast<BehaviorTreeDataModel*>(node->nodeDataModel());
    QString text = child_bt ? child_bt->instanceName() : QStringLiteral("<node>");

    QColor captionColor = child_bt ? GetCaptionColorForModel(child_bt->model(), QColor("#888888"))
                                   : QColor("#888888");
    // Default background: semi-transparent tint based on caption color
    QString defaultBg = QString("rgba(%1,%2,%3,%4)")
                            .arg(captionColor.red())
                            .arg(captionColor.green())
                            .arg(captionColor.blue())
                            .arg(110); // ~43% opacity

    auto token = new QFrame();
    token->setObjectName("inline_token");
    token->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    token->setStyleSheet(QString(
        "QFrame#inline_token { border: 1px solid %1; border-radius: 4px; background-color: %2; }")
        .arg(captionColor.name()).arg(defaultBg));
    token->setAttribute(Qt::WA_StyledBackground, true);

    auto v = new QVBoxLayout(token);
    const bool compact = (maxDepth == 0);
    v->setContentsMargins(compact ? 6 : 8,
                          compact ? 2 : 6,
                          compact ? 6 : 8,
                          compact ? 2 : 6);
    v->setSpacing(compact ? 1 : 4);
    if (compact)
    {
        // Let the layout shrink-wrap the single-line content
        v->setSizeConstraint(QLayout::SetMinimumSize);
    }

    // Header row: label
    auto header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(4);

    QFont font; font.setPointSize(10);
    auto label = new QLabel(text);
    label->setFont(font);
    label->setAlignment(Qt::AlignLeft);
    label->setStyleSheet("color: white; background: transparent;");
    label->setSizePolicy(QSizePolicy::Expanding, compact ? QSizePolicy::Fixed : QSizePolicy::Minimum);
    // Base minimums for header line
    QFontMetrics fm(font);
    int header_h = fm.height() + (compact ? 2 : 6);
    label->setMinimumHeight(fm.height());
    header->addWidget(label);
    header->addStretch();
    v->addLayout(header);

    // Store mapping for live status updates
    _inline_tokens.insert(node->id(), token);

    // Recurse into children, if any
    auto grandchildren = getChildren(*scene, *node, true);
    if (!grandchildren.empty() && (maxDepth < 0 || depth < maxDepth))
    {
        // Container with indent for children
        auto childContainer = new QWidget();
        childContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto childLayout = new QVBoxLayout(childContainer);
        int indent = 12 + depth * 12;
        childLayout->setContentsMargins(indent, compact ? 1 : 4, 4, compact ? 3 : 8);
        childLayout->setSpacing(compact ? 1 : 4);

        for (auto gc : grandchildren)
        {
            auto gcToken = buildInlineTokenForNode(gc, scene, depth + 1, maxDepth);
            if (gcToken)
            {
                childLayout->addWidget(gcToken);
            }
        }
        childContainer->layout()->activate();
        childContainer->adjustSize();
        v->addWidget(childContainer);
    }

    // Make sure token height is constrained to one text line (plus padding) in compact mode
    v->activate();
    token->adjustSize();
    if (compact)
    {
        int one_line_h = fm.height() + 4; // 1 line + small padding
        token->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        token->setMinimumHeight(one_line_h);
        label->setFixedHeight(fm.height());
    }
    else
    {
        int content_h = token->sizeHint().height();
        int min_h = std::max(16, std::max(header_h, content_h));
        token->setMinimumHeight(min_h);
    }

    return token;
}

void BehaviorTreeDataModel::toggleCollapsed()
{
    setCollapsed(!_collapsed);
}

void BehaviorTreeDataModel::cycleCollapseMode()
{
    // Cycle: Expanded -> Nested -> DirectChildren -> Expanded ...
    if (!_collapsed)
    {
        _collapse_nested = true;
        setCollapsed(true);
    }
    else if (_collapse_nested)
    {
        _collapse_nested = false;
        setCollapsed(true);
    }
    else
    {
        setCollapsed(false);
    }
}

void BehaviorTreeDataModel::updateChildTokenStatus(const QtNodes::Node& child, NodeStatus status)
{
    if (!_collapsed) return;
    if (!_inline_container || !_inline_container->isVisible())
    {
        // Only update when collapsed and tokens are visible
    }
    auto it = _inline_tokens.find(child.id());
    if (it == _inline_tokens.end()) return;

    auto token = it.value();
    // Determine border color from child's model type color (stable)
    auto child_model = dynamic_cast<BehaviorTreeDataModel*>(child.nodeDataModel());
    QColor typeColor = child_model ? GetCaptionColorForModel(child_model->model(), QColor("#888888"))
                                   : QColor("#888888");
    // Determine status color using existing style mapping
    auto stylePair = getStyleFromStatus(status, NodeStatus::IDLE);
    QColor statusColor = stylePair.first.NormalBoundaryColor;

    // Semi-transparent status background to keep overlay look without washing out colors
    QString bg = QString("rgba(%1,%2,%3,%4)")
                     .arg(statusColor.red())
                     .arg(statusColor.green())
                     .arg(statusColor.blue())
                     .arg(120); // ~47% opacity
    token->setStyleSheet(QString(
        "QFrame#inline_token { border: 1px solid %1; border-radius: 4px; background-color: %2; }")
        .arg(typeColor.name()).arg(bg));
    token->setAttribute(Qt::WA_StyledBackground, true);
}
