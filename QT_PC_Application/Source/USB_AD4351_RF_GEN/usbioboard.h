#ifndef USBIOBOARD_H
#define USBIOBOARD_H

#include <QMainWindow>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QElapsedTimer>
#include "hid_pnp.h"
#include "adf4351.h"
#include <QTimer>

#define MACRO_MAX_STEPS 14
#define MACRO_STEPS_PER_BLOCK 7

namespace Ui {
    class USB_ADF4351_form;
}

class USBIOBoard : public QMainWindow
{
    Q_OBJECT

public:
    explicit USBIOBoard(QWidget *parent = 0);
    ~USBIOBoard();

private:
    Ui::USB_ADF4351_form *ui;
    HID_PnP *plugNPlay;
    ADF4351 *adf4351;
    QTimer *sweep_timer;

    uint32_t serialNumber = 0;
    bool isIdentifyCalled = false;
    bool isAutoStartEnabled = false;
    bool isFlashProgramPending = false;
    bool isEraseFlashRequested = false;
    bool isWriteSerialNumberRequested = false;
    bool enable_auto_tx = false;
    bool isDeviceConnected = false;
    bool lastKnownRFOn = false;
    uint8_t deviceFirmwareMajor = 0;
    void getDataFromUI();
    void showEvent(QShowEvent *event);
    bool isSelctedDeviceChange;
    QString selected_usb_device;
    QStringList usb_device_list;
    bool usb_device_list_poplated;

    /* Macro tab widgets */
    QTableWidget    *macroTable;
    QPushButton     *pushButton_macroProgram;
    QPushButton     *pushButton_macroStart;
    QPushButton     *pushButton_macroStop;
    QCheckBox       *checkBox_macroLoop;
    QLabel          *label_macroStatus;
    QLabel          *label_macroStepElapsed;

    /* Macro table cell widgets - arrays of 14 */
    QCheckBox       *macroActiveCheck[MACRO_MAX_STEPS];
    QDoubleSpinBox  *macroFreqSpinBox[MACRO_MAX_STEPS];
    QSpinBox        *macroTimeSpinBox[MACRO_MAX_STEPS];
    QComboBox       *macroRFCombo[MACRO_MAX_STEPS];
    QCheckBox       *macroPLLCheck[MACRO_MAX_STEPS];

    /* Macro execution state */
    QTimer          *macro_timer;
    QTimer          *macro_elapsed_timer;
    QElapsedTimer    macroStepElapsed;
    bool            isMacroRunning;
    bool            macroFirstIteration;
    int             currentMacroStep;
    bool            enable_auto_tx_before_macro;
    bool            enable_auto_tx_before_sweep;

    /* Macro flash programming state */
    bool            isMacroProgramPending = false;
    devicemacro_s   macroBlock0;
    devicemacro_s   macroBlock1;

    void buildMacroTab();
    void updateDeviceDiagram(bool rfOn);
    void updateSweepInfo();
    void setupAdaptiveSteps();
    void setupTooltips();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void executeMacroStep();
    void highlightMacroRow(int row);
    void clearMacroHighlight();
    void packMacroData(devicemacro_s *block0, devicemacro_s *block1);
    void recalcMacroPLL();
    void updateMacroRowState(int row);
    int  getMacroTotalRange();

signals:
   void signal_update_io(uint16_t tris, uint16_t ansel,uint16_t alternate , uint16_t drive );
   void signal_update_pwm(uint16_t *duty,long frequency);
   void signal_update_reg(const uint32_t *reg, bool isStartOfSweep);
   void signal_auto_tx();
   void signal_set_RF_state(bool state);
   void signal_write_macro();

   void singal_recalculate();
   void signal_update_RF_CTRL();
   void signal_update_dac(uint16_t value);
   void signal_update_ref(uint16_t adc, uint16_t dac );

public slots:
     void update_gui(bool isConnected, UI_Data *ui_data);
     void display_reg();
     void sweep_timer_timeout();

     void program_serial_click();
     void erase_flash_click();
     void program_flash_click();
     void autotx_clicked();
     void autoStartonBoot_clicked();
     void sweep_start_click();
     void sweep_stop_click();
     void idenfity_click();
     void update_dac();
     void update_reg();
     void recalculate();
     void update_RF_CTRL();
     void comboBox_device_selection_changed();

     /* Macro slots */
     void macro_program_click();
     void macro_start_click();
     void macro_stop_click();
     void macro_timer_timeout();
     void macro_active_changed();
     void macro_freq_changed();
     void onSpinBoxCursorMoved();
     void onDiagramLinkClicked(const QString &link);
     void macro_elapsed_timer_timeout();
};

//#define PRODUCTION

#endif // USBIOBOARD_H
