#include "joystick_manager.h"
#include "ui_joystick_manager.h"

#include <QTextBrowser>

#include <QLabel>

//this function applies linear maping for transforming anything in [in_min, in_max] into [out_min, out, max] range
template <typename T>
inline T map2map(T in, T in_min, T in_max, T out_min, T out_max)
{
    return out_min + (out_max - out_min) / (in_max - in_min) * (in - in_min);
}

JoystickAxisBar::JoystickAxisBar(QWidget* parent, int joystick_, int axis_, double value)
    : QProgressBar(parent), joystick(joystick_), axis(axis_)
{
    setRange(1000,2000);
    setValue(1000);
    setTextVisible(true);
    setFormat("%v");
    setValue(static_cast<int>(map(qMax(qMin(value, max_val),min_val))));
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
}
void JoystickAxisBar::update_value(const int js, const int axis_, const qreal value)
{
    if (js == joystick && axis_ == axis)
    {
        if (in_calibration)
        {
            if (value > max_val) max_val = value;
            if (value < min_val) min_val = value;
        }

        setValue(static_cast<int>(map(qMax(qMin(value, max_val),min_val))));
    }
}
double JoystickAxisBar::map(double value)
{
    double tmp = map2map(value, min_val, max_val, -500.0, 500.0);
    if (reverse) return -tmp + 1500.0;
    else return tmp + 1500.0;
}
void JoystickAxisBar::set_input_range(double min_, double max_)
{
    if (max_ > min_)
    {
        max_val = max_;
        min_val = min_;
    }
}
void JoystickAxisBar::do_calibration(bool enable)
{
    if (in_calibration == enable) return;

    if (enable)
    {
        min_val = 10;
        max_val = -10;
    }
    in_calibration = enable;
}
void JoystickAxisBar::reset_calibration(void)
{
    set_input_range(-1.0, 1.0);
}
void JoystickAxisBar::set_reverse(bool reverse_)
{
    reverse = reverse_;
}

JoystickButton::JoystickButton(QWidget* parent, int joystick_, int axis_, bool pressed)
    : QCheckBox(parent), joystick(joystick_), axis(axis_)
{
    setChecked(pressed);
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
}
void JoystickButton::update_value(const int js, const int axis_, const bool pressed)
{
    if (js == joystick && axis_ == axis)
    {
        setChecked(pressed);
    }
}



Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);

    //start joysticks:
    joysticks = (QJoysticks::getInstance());
    update_joystick_list();
    connect(joysticks, &QJoysticks::countChanged, this, &Joystick_manager::update_joystick_list);
}

Joystick_manager::~Joystick_manager()
{
    delete ui;
}


void Joystick_manager::update_joystick_list(void)
{
    QStringList name_list = joysticks->deviceNames();
    if (name_list.isEmpty())
    {
        if (ui->comboBox_joysticopt->count() > 0) ui->comboBox_joysticopt->clear();
        return;
    }

    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString current_selection = ui->comboBox_joysticopt->currentText();
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
        if (name_list.contains(current_selection))
        {
            ui->comboBox_joysticopt->setCurrentIndex(name_list.indexOf(current_selection));
        }
    }
    else
    {
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
    }

}

void Joystick_manager::clear_axes(void)
{
    if (axes_container != NULL)
    {
        delete axes_container; //clear everything
        axes_container = nullptr;
    }

    //start fresh:
    axes_container = new QWidget(this);
    ui->scrollArea_axes->setWidget(axes_container);
    axes_layout = new QVBoxLayout(axes_container);
    axes_layout->setSpacing(6);
    axes_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_buttons(void)
{
    if (buttons_container != NULL)
    {
        delete buttons_container; //clear everything
        buttons_container = nullptr;
    }

    //start fresh:
    buttons_container = new QWidget();
    ui->scrollArea_buttons->setWidget(buttons_container);
    buttons_layout = new QGridLayout(buttons_container);
    buttons_layout->setSpacing(6);
    buttons_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_all(void)
{
    clear_axes();
    clear_buttons();
}


bool Joystick_manager::add_axis(int axis_id)
{
    if (axes_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QHBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Axis " + QString::number(axis_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickAxisBar* axis_slider_ = new JoystickAxisBar(horisontal_container_, current_joystick_id, axis_id, joysticks->getAxis(current_joystick_id, axis_id));
    axis_slider_->setMinimumHeight(font_metrics.height());
    axis_slider_->setMaximumHeight(font_metrics.height());
    QSizePolicy size_policy_ = text_browser_->sizePolicy();
    size_policy_.setHorizontalPolicy(QSizePolicy::Policy::Expanding);
    axis_slider_->setSizePolicy(size_policy_);
    horisontal_layout_->addWidget(axis_slider_);
    connect(joysticks, &QJoysticks::axisChanged, axis_slider_, &JoystickAxisBar::update_value);
    connect(this, &Joystick_manager::calibration_mode_toggled, axis_slider_, &JoystickAxisBar::do_calibration);
    connect(this, &Joystick_manager::reset_calibration, axis_slider_, &JoystickAxisBar::reset_calibration);

    QCheckBox* button_reverse_ = new QCheckBox(horisontal_container_);
    button_reverse_->setText("Reverse");
    connect(button_reverse_, &QCheckBox::clicked, axis_slider_, &JoystickAxisBar::set_reverse);
    horisontal_layout_->addWidget(button_reverse_);

    // horisontal_layout_->addStretch();

    axes_layout->addWidget(horisontal_container_);
    return true;
}

bool Joystick_manager::add_button(int button_id)
{
    if (buttons_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Button " + QString::number(button_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickButton* button_state_ = new JoystickButton(horisontal_container_, current_joystick_id, button_id, joysticks->getButton(current_joystick_id, button_id));
    connect(joysticks, &QJoysticks::buttonChanged, button_state_, &JoystickButton::update_value);
    horisontal_layout_->addWidget(button_state_);

    // horisontal_layout_->addStretch();
    int max_num_columns_ = 5;
    if (button_id < max_num_columns_) buttons_layout->addWidget(horisontal_container_, 0, button_id);
    else buttons_layout->addWidget(horisontal_container_, button_id / max_num_columns_, button_id % max_num_columns_);

    return true;
}

void Joystick_manager::on_comboBox_joysticopt_currentTextChanged(const QString &arg1)
{
    clear_all();

    if (arg1.isEmpty()) return;

    if (joysticks->count() > 0) //let's find the joystick index
    {
        QStringList name_list = joysticks->deviceNames();
        if (name_list.isEmpty() || !name_list.contains(arg1)) return;
        current_joystick_id = name_list.indexOf(arg1);
        if (!joysticks->joystickExists(current_joystick_id)) return;
    } else return;

    for (int i = 0; i < joysticks->getNumAxes(current_joystick_id); i++) add_axis(i);
    for (int i = 0; i < joysticks->getNumButtons(current_joystick_id); i++) add_button(i);

}


void Joystick_manager::on_pushButton_reset_clicked()
{
    emit reset_calibration();
}


void Joystick_manager::on_pushButton_save_clicked()
{

}


void Joystick_manager::on_pushButton_cal_axes_toggled(bool checked)
{
    emit calibration_mode_toggled(checked);
}
