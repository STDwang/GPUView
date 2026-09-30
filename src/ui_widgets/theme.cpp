/// @file theme.cpp
/// @brief 深色工作台样式，显式区分焦点、选择、禁用、悬停和滚动滑块。
#include "ui_widgets/theme.h"
namespace gpuview {
/// 仅影响绘制外观，不修改模型、查询和缓存键。
QString darkTheme() {
    return QStringLiteral(R"(
QMainWindow,QWidget { background:#101923; color:#d7e4ee; }
QToolBar { background:#1b2a38; padding:6px; spacing:8px; border-bottom:1px solid #304555; }
QToolButton,QPushButton { padding:6px 10px; border:1px solid #415e73; border-radius:5px; background:#182735; }
QToolButton:hover,QPushButton:hover { background:#29485e; border-color:#7199b5; }
QToolButton:pressed,QPushButton:pressed { background:#31586e; }
QToolButton:disabled,QPushButton:disabled { color:#7e8e9b; background:#15202b; border-color:#293947; }
QPushButton:focus,QToolButton:focus { border:1px solid #55dabb; }
QDockWidget::title { background:#203343; padding:7px; font-weight:600; }
QMainWindow::separator { background:#243949; width:5px; height:5px; }
QMainWindow::separator:hover { background:#55dabb; }
QTextBrowser { background:#162330; border:1px solid #2e4353; border-radius:4px; padding:8px; }
QStatusBar { background:#172532; border-top:1px solid #304555; }
QLineEdit,QComboBox,QDoubleSpinBox { padding:5px; border:1px solid #415e73; border-radius:4px; background:#0d1721; selection-background-color:#315f76; }
QLineEdit:focus,QComboBox:focus,QDoubleSpinBox:focus { border-color:#55dabb; }
QLineEdit:disabled,QDoubleSpinBox:disabled { color:#7e8e9b; border-color:#293947; }
QCheckBox { spacing:6px; padding:3px; }
QTreeWidget,QTableView { background:#111e29; alternate-background-color:#172a38; border:1px solid #304555; selection-background-color:#315c73; selection-color:#ffffff; gridline-color:#273d4e; }
QTableView::item { padding:3px 6px; }
QTableView::item:selected,QTreeWidget::item:selected { background:#315c73; color:#ffffff; }
QTableView::item:hover,QTreeWidget::item:hover { background:#253e50; }
QHeaderView::section { background:#203343; color:#dceaf3; padding:5px 7px; border:0; border-right:1px solid #365063; border-bottom:1px solid #365063; }
QTableCornerButton::section { background:#203343; border:1px solid #365063; }
QTabWidget::pane { border:1px solid #304555; }
QTabBar::tab { padding:7px 12px; background:#152330; color:#a8bdcd; border-bottom:2px solid transparent; }
QTabBar::tab:selected { background:#234354; color:#f0fafc; border-bottom:2px solid #55dabb; }
QTabBar::tab:hover { background:#294353; }
QScrollBar:vertical { background:#0b121a; width:20px; margin:0; border:1px solid #35495a; border-radius:6px; }
QScrollBar::handle:vertical { background:#7896ad; min-height:40px; margin:2px; border-radius:5px; }
QScrollBar::handle:vertical:hover { background:#a5c9e0; }
QScrollBar::handle:vertical:pressed { background:#55dabb; }
QScrollBar::handle:vertical:disabled { background:#243442; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; border:0; background:transparent; }
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
QScrollBar:horizontal { background:#0b121a; height:16px; border:1px solid #35495a; }
QScrollBar::handle:horizontal { background:#7896ad; min-width:40px; margin:2px; border-radius:4px; }
QScrollBar::handle:horizontal:hover { background:#a5c9e0; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; background:transparent; }
QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal { background:transparent; }
QProgressBar { border:1px solid #35495a; border-radius:4px; text-align:center; background:#0d1721; }
QProgressBar::chunk { background:#2d8e7b; }
QMenu { background:#192c3b; border:1px solid #415e73; padding:4px; }
QMenu::item { padding:6px 22px; }
QMenu::item:selected { background:#315c73; }
QToolTip { background:#243e50; color:#f0fafc; border:1px solid #7199b5; padding:6px; }
)");
}
}
