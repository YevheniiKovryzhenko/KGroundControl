#ifndef JOYSTICK_MANAGER_H
#define JOYSTICK_MANAGER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QCheckBox>

#include "joystick.h"


class JoystickAxisBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit JoystickAxisBar(QWidget* parent, int joystick, int axis, double value);

public slots:
    void update_value(const int js, const int axis, const qreal value);
    void set_input_range(double min, double max);
    void do_calibration(bool enable);
    void reset_calibration(void);
    void set_reverse(bool reversed);

private:
    double map(double value);

    const int joystick; /**< The numerical ID of the joystick */
    const int axis; /**< The numerical ID of the axis */
    double min_val = -1.0;
    double max_val = 1.0;
    bool reverse = false;

    bool in_calibration = false;

};

class JoystickButton : public QCheckBox
{
    Q_OBJECT
public:
    explicit JoystickButton(QWidget* parent, int joystick, int axis, bool pressed);

public slots:
    void update_value(const int js, const int axis, const bool pressed);

private:
    const int joystick; /**< The numerical ID of the joystick */
    const int axis; /**< The numerical ID of the axis */
};

namespace Ui {
class Joystick_manager;
}

class Joystick_manager : public QWidget
{
    Q_OBJECT

public:
    explicit Joystick_manager(QWidget *parent = nullptr);
    ~Joystick_manager();

signals:
    void calibration_mode_toggled(bool enabled);
    void reset_calibration(void);

private slots:
    void update_joystick_list(void);

    void on_comboBox_joysticopt_currentTextChanged(const QString &arg1);

    bool add_axis(int axis_id);
    bool add_button(int button_id);

    void clear_all(void);
    void clear_axes(void);
    void clear_buttons(void);

    void on_pushButton_reset_clicked();
    void on_pushButton_save_clicked();
    void on_pushButton_cal_axes_toggled(bool checked);

private:
    int current_joystick_id = 0;

    Ui::Joystick_manager *ui;

    QJoysticks *joysticks = nullptr;
    QWidget *axes_container = nullptr, *buttons_container = nullptr;
    QVBoxLayout *axes_layout = nullptr;
    QGridLayout *buttons_layout = nullptr;
};

#endif // JOYSTICK_MANAGER_H
