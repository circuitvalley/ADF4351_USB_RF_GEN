#include "usbioboard.h"
#include "ui_usbio.h"
#include "adf4351.h"

USBIOBoard::USBIOBoard(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::USB_ADF4351_form)
{
    ui->setupUi(this);

    plugNPlay = new HID_PnP();
    adf4351 = new ADF4351();
    this->enable_auto_tx = false;
   // connect(ui->spinBox_pwm_freq,SIGNAL(valueChanged(int)),this,SLOT(update_pwm()));

    connect(this, SIGNAL(singal_recalculate()), adf4351, SLOT(BuildRegisters()));

    connect(this, SIGNAL(signal_update_reg(const uint32_t *, bool )), plugNPlay, SLOT(change_reg(const uint32_t *, bool)));
    connect(this, SIGNAL(signal_update_RF_CTRL()), plugNPlay, SLOT(change_RF_CTRL()));
    connect(this, SIGNAL(signal_auto_tx()), this, SLOT(update_reg()));

    connect(ui->USBTX, SIGNAL(clicked(bool)), this, SLOT(update_reg()));

    //connect(ui->groupBox_main, SIGNAL(clicked(bool)), this, SLOT(recalculate()));

    connect(ui->RF_CTRL, SIGNAL(clicked(bool)), this, SLOT(update_RF_CTRL()));

    connect(plugNPlay, SIGNAL(hid_comm_update(bool, UI_Data*)), this, SLOT(update_gui(bool, UI_Data*)));
    connect(adf4351, SIGNAL(reg_update_result()), this, SLOT(display_reg()));
    connect(ui->pushButton_sweep_start, SIGNAL(clicked(bool)), this, SLOT(sweep_start_click()));
    connect(ui->pushButton_sweep_stop, SIGNAL(clicked(bool)), this, SLOT(sweep_stop_click()));


    connect(ui->pushButton_program_serial, SIGNAL(clicked(bool)), this, SLOT(program_serial_click()));
    connect(ui->pushButton_erase_flash, SIGNAL(clicked(bool)), this, SLOT(erase_flash_click()));
    connect(ui->pushButton_program_flash, SIGNAL(clicked(bool)), this, SLOT(program_flash_click()));
    connect(ui->pushButton_identify, SIGNAL(clicked(bool)), this, SLOT(idenfity_click()));

    connect(ui->checkBox_autotx, SIGNAL(clicked(bool)), this, SLOT(autotx_clicked()));
    connect(ui->checkBox_autoLockOnBoot, SIGNAL(clicked(bool)), this, SLOT(autoStartonBoot_clicked()));
    connect(ui->checkBox_prog_enable_sweep, SIGNAL(clicked(bool)), this, SLOT(recalculate()));

    connect(ui->doubleSpinBox_freq, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_freq_basic, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_sweep_start, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_sweep_end, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));


    connect(ui->comboBox_auxsetting, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->groupBox_main, SIGNAL(clicked(bool)), this, SLOT(recalculate()));
    connect(ui->comboBox_ABP, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_mode, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_mux_out, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_MTLD, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_band_select_clk_mode, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_AUX_EN, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_AUX_OUT, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_AUX_out_power, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_phase_adjust, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_double_buff, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_charge_cancellation, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_charge_pump_current, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_counter_rst, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_cp_3_state, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_CLK_div_mode, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_CSR, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_feedback, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_LDF, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_LDP, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_LDPIN, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_powerdown, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_presacler, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_PD_polarity, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_vco_powerdown, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_RF_OUT, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->comboBox_RF_POWER, SIGNAL(currentIndexChanged(int)), this , SLOT(recalculate()));
    connect(ui->spinBox_clock_divider, SIGNAL(valueChanged(int)), this, SLOT(recalculate()));
    connect(ui->spinBox_rcount, SIGNAL(valueChanged(int)), this, SLOT(recalculate()));
    connect(ui->spinBox_phase_val, SIGNAL(valueChanged(int)), this, SLOT(recalculate()));
    connect(ui->checkBox_refdiv2, SIGNAL(clicked(bool)), this, SLOT(recalculate()));
    connect(ui->checkBox_refx2, SIGNAL(clicked(bool)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_ref, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->comboBox_usb_devices, SIGNAL(currentIndexChanged(int)),this, SLOT(comboBox_device_selection_changed()));



    QFont font = ui->spinBox_serial->font();
    font.setCapitalization(QFont::AllUppercase);
    ui->spinBox_serial->setFont(font);

    #ifndef PRODUCTION
    ui->spinBox_serial->setVisible(false);
    ui->pushButton_program_serial->setVisible(false);
    #endif

    sweep_timer = new QTimer();

    this->usb_device_list_poplated = false;
    this->selected_usb_device = QString();
    connect(sweep_timer, SIGNAL(timeout()), this, SLOT(sweep_timer_timeout()));
    this->recalculate();

}

void USBIOBoard::showEvent( QShowEvent *event )
{
// call whatever your base class is!

    this->recalculate();
}

USBIOBoard::~USBIOBoard()
{
 //   disconnect(this, SIGNAL(tris_box_state_changed()),plugNPlay,SLOT(toggle_io()));

    disconnect(plugNPlay, SIGNAL(hid_comm_update(bool,UI_Data)), this, SLOT(update_gui(bool, UI_Data)));
    delete ui;
    delete plugNPlay;
}

void USBIOBoard::update_dac()
{
   // emit signal_update_dac(ui->spinBox_dac->value());
}
void USBIOBoard::getDataFromUI()
{
    if ( ui->tabWidget->currentIndex() == 0) //select value from advance or basic tab
    {
        ui->doubleSpinBox_freq->setValue(ui->doubleSpinBox_freq_basic->value());
    }
    else
    {
        ui->doubleSpinBox_freq_basic->setValue(ui->doubleSpinBox_freq->value());
    }

    this->adf4351->frequency        = ui->doubleSpinBox_freq->value();
    this->adf4351->start_freq       = ui->doubleSpinBox_sweep_start->value();
    this->adf4351->stop_freq        = ui->doubleSpinBox_sweep_end->value();
    this->adf4351->step_freq        = ui->doubleSpinBox_sweep_step->value();
    this->adf4351->step_ms          = ui->spinBox_sweep_delay->value();
    this->adf4351->aux_select       = static_cast<uint16_t>(ui->comboBox_auxsetting->currentIndex());
    this->adf4351->isStartOnBoot    = ui->checkBox_autoLockOnBoot->isChecked();
    this->adf4351->isSweepEnabled   = ui->checkBox_prog_enable_sweep->isChecked();

    this->adf4351->ref_freq = ui->doubleSpinBox_ref->value();
    this->adf4351->r_counter = ui->spinBox_rcount->text().toInt();
    this->adf4351->PHASE = ui->spinBox_phase_val->text().toInt();
    this->adf4351->PHASE_ADJUST = ui->comboBox_phase_adjust->currentIndex();
    this->adf4351->ref_div2 = ui->checkBox_refdiv2->isChecked();
    this->adf4351->ref_doubler = ui->checkBox_refx2->isChecked();
    this->adf4351->low_noise_spur_mode = ui->comboBox_mode->currentIndex();
    this->adf4351->muxout = ui->comboBox_mux_out->currentIndex();
    this->adf4351->double_buff = ui->comboBox_double_buff->currentIndex();
    this->adf4351->charge_pump_current = ui->comboBox_charge_pump_current->currentIndex();
    this->adf4351->LDF = ui->comboBox_LDF->currentIndex();
    this->adf4351->LDP = ui->comboBox_LDP->currentIndex();
    this->adf4351->PD_Polarity = ui->comboBox_PD_polarity->currentIndex();
    this->adf4351->cp_3stage = ui->comboBox_cp_3_state->currentIndex();
    this->adf4351->counter_reset = ui->comboBox_counter_rst->currentIndex();
    this->adf4351->band_select_clock_mode = ui->comboBox_band_select_clk_mode->currentIndex();
    this->adf4351->charge_cancelletion = ui->comboBox_charge_cancellation->currentIndex();
    this->adf4351->ABP = ui->comboBox_ABP->currentIndex();
    this->adf4351->CSR = ui->comboBox_CSR->currentIndex();
    this->adf4351->clock_divider = ui->spinBox_clock_divider->value();
    this->adf4351->CLK_DIV_MODE = ui->comboBox_CLK_div_mode->currentIndex();
    this->adf4351->LD = ui->comboBox_LDPIN->currentIndex();
    this->adf4351->power_down = ui->comboBox_powerdown->currentIndex();
    this->adf4351->VCO_power_down = ui->comboBox_vco_powerdown->currentIndex();
    this->adf4351->MTLD = ui->comboBox_MTLD->currentIndex();
    this->adf4351->AUX_output_mode = ui->comboBox_AUX_OUT->currentIndex();
    this->adf4351->AUX_output_enable = ui->comboBox_AUX_EN->currentIndex();
    this->adf4351->AUX_output_power = ui->comboBox_AUX_out_power->currentIndex();
    this->adf4351->RF_output_power = ui->comboBox_RF_POWER->currentIndex();
    this->adf4351->RF_OUT = ui->comboBox_RF_OUT->currentIndex();
    this->adf4351->PR1 = ui->comboBox_presacler->currentIndex();
    this->adf4351->feedback_select = ui->comboBox_feedback->currentIndex();

    ui->doubleSpinBox_sweep_end->setMinimum(ui->doubleSpinBox_sweep_start->value());

    double max_val_step = qMin(ui->doubleSpinBox_sweep_end->value() - ui->doubleSpinBox_sweep_start->value(), 4400.00);
    if ( max_val_step < 0.0)
    {
        max_val_step = 0;
    }
    ui->doubleSpinBox_sweep_step->setMaximum(max_val_step);

    if (ui->doubleSpinBox_sweep_step->value() == 0.0 and max_val_step != 0.0) //reset value to 1 if not 0
    {
        ui->doubleSpinBox_sweep_step->setValue(1.0);
    }

}
void USBIOBoard::recalculate()
{
   this->getDataFromUI();
   emit singal_recalculate();
}

void USBIOBoard::update_reg()
{

    bool bStatus = false;
    const uint32_t hex_values[] = {
        ui->line_reg0->text().toUInt(&bStatus, 16),
        ui->line_reg1->text().toUInt(&bStatus, 16),
        ui->line_reg2->text().toUInt(&bStatus, 16),
        ui->line_reg3->text().toUInt(&bStatus, 16),
        ui->line_reg4->text().toUInt(&bStatus, 16),
        ui->line_reg5->text().toUInt(&bStatus, 16),
    };


    emit signal_update_reg(hex_values, this->adf4351->isStartOfSweep);
}

void USBIOBoard::update_RF_CTRL()
{
    emit signal_update_RF_CTRL();
}

void USBIOBoard::sweep_stop_click()
{
    this->enable_auto_tx = ui->checkBox_autotx->isChecked();
    sweep_timer->stop();
}

void USBIOBoard::sweep_start_click()
{
    this->enable_auto_tx = true;

    ui->doubleSpinBox_sweep_freq->setValue(ui->doubleSpinBox_sweep_start->value());
    sweep_timer->start(ui->spinBox_sweep_delay->value());
}

void USBIOBoard::autotx_clicked()
{
    this->enable_auto_tx = ui->checkBox_autotx->isChecked();
    ui->USBTX->setEnabled(!this->enable_auto_tx);
}

void USBIOBoard::autoStartonBoot_clicked()
{
    this->isAutoStartEnabled    = ui->checkBox_autoLockOnBoot->isChecked();
    emit singal_recalculate();
}

void USBIOBoard::idenfity_click()
{
    this->isIdentifyCalled = true;
}

void USBIOBoard::program_serial_click()
{
    this->isWriteSerialNumberRequested = true;
    this->serialNumber = static_cast<uint32_t>(ui->spinBox_serial->value());
}


void USBIOBoard::erase_flash_click()
{
    this->isEraseFlashRequested = true;
}

void USBIOBoard::program_flash_click()
{
    this->isFlashProgramPending = true;
    this->isAutoStartEnabled    = ui->checkBox_autoLockOnBoot->isChecked();
    emit singal_recalculate();
    update_reg();
}

void USBIOBoard::sweep_timer_timeout()
{
    const double frequency = ui->doubleSpinBox_sweep_freq->text().toDouble() + ui->doubleSpinBox_sweep_step->text().toDouble();
    ui->doubleSpinBox_sweep_freq->setValue(frequency);
    this->adf4351->frequency = ui->doubleSpinBox_sweep_freq->text().toDouble();
    this->adf4351->isStartOfSweep = false;


    if (frequency > ui->doubleSpinBox_sweep_end->value())
    {
         if (ui->checkBox_sweep_loop->isChecked())
        {
            ui->doubleSpinBox_sweep_freq->setValue(ui->doubleSpinBox_sweep_start->value());
            this->adf4351->isStartOfSweep = true;
        }
        else
        {
            this->sweep_stop_click();
        }
    }

    emit singal_recalculate();
}

void USBIOBoard::display_reg()
{
    ui->line_reg0->setText(QString::number(this->adf4351->reg[0],16).toUpper());
    ui->line_reg1->setText(QString::number(this->adf4351->reg[1],16).toUpper());
    ui->line_reg2->setText(QString::number(this->adf4351->reg[2],16).toUpper());
    ui->line_reg3->setText(QString::number(this->adf4351->reg[3],16).toUpper());
    ui->line_reg4->setText(QString::number(this->adf4351->reg[4],16).toUpper());
    ui->line_reg5->setText(QString::number(this->adf4351->reg[5],16).toUpper());

    if (this->enable_auto_tx)
    {
        emit signal_auto_tx();
    }
}

void USBIOBoard::update_gui(bool isConnected, UI_Data *ui_data)
{
    ui->label_device_busy->setVisible(false);

    if(isConnected)
    {
        if (!ui_data->isReadFirmwareInfoPending   )
        {
            this->setWindowTitle("RFGEN44 RF GEN: FW : " + QString::number( ui_data->firmware_version_major ) +"." + QString::number( ui_data->firmware_version_minor ) + ":" +  QString::number( ui_data->firmware_build_number )  + " Serial: " +  ui_data->selected_usb_device + " Connected");
        }

        if (!ui_data->isReadFirmwareInfoPending)
        {

            if (ui_data->RF_OUT)
            {
                ui->RF_CTRL->setText("RF : ON");
            }else
            {
                ui->RF_CTRL->setText("RF : OFF");
            }

            if (ui_data->device_busy)
            {
                ui->label_device_busy->setVisible(true);
            }
            else
            {
                ui->label_device_busy->setVisible(false);
            }
        }
    }
    else
    {
        ui_data->readRFCTRL_pending = true;
        ui_data->isReadFirmwareInfoPending = true;
        // stop all timers here
        this->setWindowTitle("RFGEN44 RF GEN : Device Not Found");
        ui->RF_CTRL->setText("RF : XX");
        sweep_timer->stop();
    }

    this->usb_device_list = ui_data->usb_device_list;
    if (ui_data->isDeviceListChanged == true )
    {

        this->usb_device_list_poplated = false;
        ui->comboBox_usb_devices->clear();
        ui->comboBox_usb_devices->addItems(this->usb_device_list);
        if (this->selected_usb_device.length() > 0) // if there any device slected
        {
            int current_opened_device_index = ui->comboBox_usb_devices->findText(this->selected_usb_device);
            if (current_opened_device_index >= 0)
            {
                ui->comboBox_usb_devices->setCurrentIndex(current_opened_device_index);
            }
            else
            {
                this->selected_usb_device = this->usb_device_list.first();
                this->isSelctedDeviceChange = true;
            }
        }
        else
        {
            this->selected_usb_device = this->usb_device_list.first();
            this->isSelctedDeviceChange = true;
        }

        this->usb_device_list_poplated = true;
    }

    ui_data->selected_usb_device = this->selected_usb_device;
    ui_data->isSelctedDeviceChange = this->isSelctedDeviceChange;
    if (ui_data->isDeviceChangeDone == true)
    {

        this->isSelctedDeviceChange = false;
        ui_data->isDeviceChangeDone = false;
        emit singal_recalculate();
    }

    if (this->isFlashProgramPending)
    {
        this->isFlashProgramPending = false;
        ui_data->isFlashWriteRequested = true;
        ui_data->isDeviceCtrlPending = true;
    }
    ui_data->autoStartatBoot = this->isAutoStartEnabled;

    if (this->isIdentifyCalled)
    {
        this->isIdentifyCalled = false;
        ui_data->isIdentfiyLEDRequested = true;
        ui_data->isDeviceCtrlPending = true;
    }

    ui_data->adf4351.frequency      = static_cast<uint32_t>(this->adf4351->frequency * 100); //multiply by 100 to make fix point
    ui_data->adf4351.start_freq     = static_cast<uint32_t>(this->adf4351->start_freq * 100); //multiply by 100 to make fix point
    ui_data->adf4351.stop_freq      = static_cast<uint32_t>(this->adf4351->stop_freq * 100); //multiply by 100 to make fix point
    ui_data->adf4351.step_freq      = static_cast<uint32_t>(this->adf4351->step_freq * 100); //multiply by 100 to make fix point
    ui_data->adf4351.step_ms        = static_cast<uint16_t>(this->adf4351->step_ms);
    ui_data->adf4351.aux_select     = static_cast<uint16_t>(this->adf4351->aux_select);
    ui_data->adf4351.isSweepEnabled = static_cast<uint8_t>(this->adf4351->isSweepEnabled);
    ui_data->adf4351.isStartOnBoot  = static_cast<uint8_t>(this->adf4351->isStartOnBoot);
    ui_data->adf4351.isStartOfSweep = static_cast<uint8_t>(this->adf4351->isStartOfSweep);
    ui_data->adf4351.ref_freq       = static_cast<uint32_t>(this->adf4351->ref_freq * 100);

    if ( this->isEraseFlashRequested)
    {
        this->isEraseFlashRequested = false;
        ui_data->isEraseFlashRequested = true;
    }

    if (this->isWriteSerialNumberRequested)
    {
        this->isWriteSerialNumberRequested = false;
        ui_data->isWriteSerialNumberRequested = true;
    }
    ui_data->deviceinfo.serialNumber           = this->serialNumber;
}

void USBIOBoard::comboBox_device_selection_changed()
{
   if (this->usb_device_list_poplated and ui->comboBox_usb_devices->count() != 0)
   {
        this->selected_usb_device = ui->comboBox_usb_devices->currentText();
        this->isSelctedDeviceChange = true;
   }

}

