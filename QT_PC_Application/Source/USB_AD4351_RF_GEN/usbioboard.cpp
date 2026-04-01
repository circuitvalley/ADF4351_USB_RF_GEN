/*******************************************************************************
 *  usbioboard.cpp
 *
 *  Main UI controller for RFGEN44 ADF4351 Signal Generator.
 *  Handles Control/Advanced tabs (via .ui), Macro tab (built programmatically),
 *  USB device management, sweep, and macro execution.
 ******************************************************************************/

#include "usbioboard.h"
#include "ui_usbio.h"
#include "adf4351.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QtMath>
#include <QPainter>
#include <QPixmap>


/*******************************************************************************
 *  Constructor
 ******************************************************************************/
USBIOBoard::USBIOBoard(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::USB_ADF4351_form)
{
    ui->setupUi(this);

    plugNPlay = new HID_PnP();
    adf4351   = new ADF4351();

    this->enable_auto_tx            = true;
    this->isMacroRunning            = false;
    this->currentMacroStep          = 0;
    this->enable_auto_tx_before_macro = false;
    this->enable_auto_tx_before_sweep = false;

    /* ---- Signal/Slot connections: ADF4351 register pipeline ---- */
    connect(this, SIGNAL(singal_recalculate()),
            adf4351, SLOT(BuildRegisters()));

    connect(this, SIGNAL(signal_update_reg(const uint32_t *, bool)),
            plugNPlay, SLOT(change_reg(const uint32_t *, bool)));

    connect(this, SIGNAL(signal_update_RF_CTRL()),
            plugNPlay, SLOT(change_RF_CTRL()));

    connect(this, SIGNAL(signal_set_RF_state(bool)),
            plugNPlay, SLOT(set_RF_state(bool)));

    connect(this, SIGNAL(signal_write_macro()),
            plugNPlay, SLOT(write_macro()));

    connect(this, SIGNAL(signal_auto_tx()),
            this, SLOT(update_reg()));

    /* ---- Signal/Slot connections: UI buttons ---- */
    connect(ui->USBTX,   SIGNAL(clicked(bool)), this, SLOT(update_reg()));
    connect(ui->RF_CTRL,  SIGNAL(clicked(bool)), this, SLOT(update_RF_CTRL()));
    connect(ui->label_deviceDiagram, SIGNAL(linkActivated(QString)),
            this, SLOT(onDiagramLinkClicked(QString)));

    /* ---- Signal/Slot connections: HID device ---- */
    connect(plugNPlay, SIGNAL(hid_comm_update(bool, UI_Data*)),
            this, SLOT(update_gui(bool, UI_Data*)));

    connect(adf4351, SIGNAL(reg_update_result()),
            this, SLOT(display_reg()));

    /* ---- Signal/Slot connections: Sweep ---- */
    connect(ui->pushButton_sweep_start, SIGNAL(clicked(bool)), this, SLOT(sweep_start_click()));
    connect(ui->pushButton_sweep_stop,  SIGNAL(clicked(bool)), this, SLOT(sweep_stop_click()));

    /* ---- Signal/Slot connections: Device settings ---- */
    connect(ui->pushButton_program_serial, SIGNAL(clicked(bool)), this, SLOT(program_serial_click()));
    connect(ui->pushButton_erase_flash,    SIGNAL(clicked(bool)), this, SLOT(erase_flash_click()));
    connect(ui->pushButton_program_flash,  SIGNAL(clicked(bool)), this, SLOT(program_flash_click()));
    ui->pushButton_program_flash->setEnabled(false);
    ui->pushButton_program_flash->setToolTip("Waiting for device connection...");
    ui->pushButton_erase_flash->setEnabled(false);
    ui->pushButton_erase_flash->setToolTip("Waiting for device connection...");
    ui->USBTX->setEnabled(false);
    ui->USBTX->setToolTip("Waiting for device connection...");
    ui->RF_CTRL->setEnabled(false);
    ui->RF_CTRL->setToolTip("Waiting for device connection...");
    ui->pushButton_sweep_start->setEnabled(false);
    ui->pushButton_sweep_start->setToolTip("Waiting for device connection...");
    ui->pushButton_sweep_stop->setEnabled(false);
    ui->pushButton_sweep_stop->setToolTip("Waiting for device connection...");
    connect(ui->pushButton_identify,       SIGNAL(clicked(bool)), this, SLOT(idenfity_click()));
    ui->pushButton_identify->setEnabled(false);
    ui->pushButton_identify->setToolTip("Waiting for device connection...");
    connect(ui->checkBox_autotx,           SIGNAL(clicked(bool)), this, SLOT(autotx_clicked()));
    connect(ui->checkBox_autoLockOnBoot,   SIGNAL(clicked(bool)), this, SLOT(autoStartonBoot_clicked()));
    connect(ui->checkBox_prog_enable_sweep,SIGNAL(clicked(bool)), this, SLOT(recalculate()));

    /* ---- Signal/Slot connections: All ADF4351 parameter widgets → recalculate ---- */
    connect(ui->doubleSpinBox_freq,        SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_freq_basic,  SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_sweep_start, SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_sweep_end,   SIGNAL(valueChanged(double)), this, SLOT(recalculate()));
    connect(ui->doubleSpinBox_ref,         SIGNAL(valueChanged(double)), this, SLOT(recalculate()));

    connect(ui->comboBox_auxsetting,           SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->groupBox_main,                 SIGNAL(clicked(bool)),            this, SLOT(recalculate()));
    connect(ui->comboBox_ABP,                  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_mode,                 SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_mux_out,              SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_MTLD,                 SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_band_select_clk_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_AUX_EN,               SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_AUX_OUT,              SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_AUX_out_power,        SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_phase_adjust,         SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_double_buff,          SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_charge_cancellation,  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_charge_pump_current,  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_counter_rst,          SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_cp_3_state,           SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_CLK_div_mode,         SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_CSR,                  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_feedback,             SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_LDF,                  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_LDP,                  SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_LDPIN,                SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_powerdown,            SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_presacler,            SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_PD_polarity,          SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_vco_powerdown,        SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_RF_OUT,               SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_RF_POWER,             SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));

    /* Sync RF power combo between Control tab and Advance tab */
    connect(ui->comboBox_RF_POWER_basic, SIGNAL(currentIndexChanged(int)), this, SLOT(recalculate()));
    connect(ui->comboBox_RF_POWER_basic, SIGNAL(currentIndexChanged(int)), ui->comboBox_RF_POWER, SLOT(setCurrentIndex(int)));
    connect(ui->comboBox_RF_POWER,       SIGNAL(currentIndexChanged(int)), ui->comboBox_RF_POWER_basic, SLOT(setCurrentIndex(int)));

    connect(ui->spinBox_clock_divider,         SIGNAL(valueChanged(int)),        this, SLOT(recalculate()));
    connect(ui->spinBox_rcount,                SIGNAL(valueChanged(int)),        this, SLOT(recalculate()));
    connect(ui->spinBox_phase_val,             SIGNAL(valueChanged(int)),        this, SLOT(recalculate()));
    connect(ui->checkBox_refdiv2,              SIGNAL(clicked(bool)),            this, SLOT(recalculate()));
    connect(ui->checkBox_refx2,                SIGNAL(clicked(bool)),            this, SLOT(recalculate()));

    connect(ui->comboBox_usb_devices, SIGNAL(currentIndexChanged(int)),
            this, SLOT(comboBox_device_selection_changed()));

    /* ---- Serial number display (production only) ---- */
    QFont font = ui->spinBox_serial->font();
    font.setCapitalization(QFont::AllUppercase);
    ui->spinBox_serial->setFont(font);

    #ifndef PRODUCTION
        ui->spinBox_serial->setVisible(false);
        ui->pushButton_program_serial->setVisible(false);
    #endif

    /* ---- Timers ---- */
    sweep_timer = new QTimer();
    macro_timer = new QTimer();
    macro_elapsed_timer = new QTimer();

    this->usb_device_list_poplated = false;
    this->selected_usb_device      = QString();

    connect(sweep_timer, SIGNAL(timeout()), this, SLOT(sweep_timer_timeout()));
    connect(macro_timer, SIGNAL(timeout()), this, SLOT(macro_timer_timeout()));
    connect(macro_elapsed_timer, SIGNAL(timeout()), this, SLOT(macro_elapsed_timer_timeout()));

    /* ---- Build the Macro tab UI and trigger initial register calc ---- */
    buildMacroTab();

    /* Reorder tabs: Control(0), Macro(1), Advance Control(2)
     * .ui order is: Control(0), Advance Control(1), Macro(2) */
    QWidget *macroTabWidget = ui->tabWidget->widget(2);
    QString  macroTabTitle  = ui->tabWidget->tabText(2);
    ui->tabWidget->removeTab(2);
    ui->tabWidget->insertTab(1, macroTabWidget, macroTabTitle);
    ui->tabWidget->setCurrentIndex(0);

    /* ---- Setup adaptive step size for all spinboxes ---- */
    setupAdaptiveSteps();
    setupTooltips();

    /* ---- Initial register calculation ---- */
    this->recalculate();
}


/*******************************************************************************
 *
 *  MACRO TAB — Build UI, step control, PLL auto-calc, execution engine
 *
 ******************************************************************************/

/**
 * @brief Builds the Macro tab UI programmatically inside the empty "tab"
 *        widget defined in the .ui file.
 *
 * Table columns: # | Active | Frequency (MHz) | Time (ms) | RF Output | Recal PLL
 *
 * - Steps 1–2 are always active (checkboxes locked).
 * - Steps 3–14 default to inactive; user enables sequentially.
 * - "Recal PLL" column is read-only, auto-calculated from frequency changes.
 */
void USBIOBoard::buildMacroTab()
{
    QWidget *macroTab = ui->tabWidget->findChild<QWidget *>("tab");
    if (!macroTab) return;

    /* ---- Main layout ---- */
    QVBoxLayout *mainLayout = new QVBoxLayout(macroTab);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    /* ---- Top row: status label ---- */
    QHBoxLayout *topRow = new QHBoxLayout();
    label_macroStatus = new QLabel("Status: Idle  |  Active Steps: 2");
    label_macroStatus->setFont(QFont(label_macroStatus->font().family(), 11, QFont::Bold));
    topRow->addWidget(label_macroStatus);
    topRow->addStretch();
    mainLayout->addLayout(topRow);

    /* ---- Macro step table ---- */
    macroTable = new QTableWidget(MACRO_MAX_STEPS, 6, macroTab);
    macroTable->setHorizontalHeaderLabels(
        QStringList() << "#" << "Active" << "Frequency (MHz)"
                      << "Time (ms)" << "RF Output" << "Recal PLL"
    );

    macroTable->horizontalHeader()->setFont(
        QFont(macroTable->font().family(), 10, QFont::Bold)
    );
    macroTable->verticalHeader()->setVisible(false);
    macroTable->setSelectionMode(QAbstractItemView::NoSelection);
    macroTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    /* Column widths — Frequency stretches to fill remaining space */
    macroTable->setColumnWidth(0, 30);      // #
    macroTable->setColumnWidth(1, 70);      // Active
    macroTable->setColumnWidth(3, 160);     // Time (ms)
    macroTable->setColumnWidth(4, 100);     // RF Output
    macroTable->setColumnWidth(5, 100);     // Recal PLL
    macroTable->horizontalHeader()->setStretchLastSection(false);
    macroTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    /* ---- Populate table rows ---- */
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        macroTable->setRowHeight(i, 28);

        /* Col 0: Step number (read-only label) */
        QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(i + 1));
        numItem->setTextAlignment(Qt::AlignCenter);
        numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
        macroTable->setItem(i, 0, numItem);

        /* Col 1: Active checkbox — centred in cell */
        QWidget     *actContainer = new QWidget();
        QHBoxLayout *actLayout    = new QHBoxLayout(actContainer);
        actLayout->setAlignment(Qt::AlignCenter);
        actLayout->setContentsMargins(0, 0, 0, 0);
        macroActiveCheck[i] = new QCheckBox();
        actLayout->addWidget(macroActiveCheck[i]);
        macroTable->setCellWidget(i, 1, actContainer);

        /* First two steps are always active and cannot be unchecked */
        if (i < 2)
        {
            macroActiveCheck[i]->setChecked(true);
            macroActiveCheck[i]->setEnabled(false);
        }
        else
        {
            macroActiveCheck[i]->setChecked(false);
        }

        /* Col 2: Frequency (MHz) — ADF4351 range 35–4400 MHz */
        macroFreqSpinBox[i] = new QDoubleSpinBox();
        macroFreqSpinBox[i]->setRange(35.0, 4400.0);
        macroFreqSpinBox[i]->setDecimals(2);
        macroFreqSpinBox[i]->setSingleStep(0.01);
        macroFreqSpinBox[i]->setValue(100.0);
        macroFreqSpinBox[i]->setSuffix(" MHz");
        macroTable->setCellWidget(i, 2, macroFreqSpinBox[i]);

        /* Col 3: Dwell time (ms) — uint24_t max ≈ 4.66 hours */
        macroTimeSpinBox[i] = new QSpinBox();
        macroTimeSpinBox[i]->setRange(1, 16777215);
        macroTimeSpinBox[i]->setValue(500);
        macroTimeSpinBox[i]->setSuffix(" ms");
        macroTable->setCellWidget(i, 3, macroTimeSpinBox[i]);

        /* Col 4: RF output — explicit On / Off per step */
        macroRFCombo[i] = new QComboBox();
        macroRFCombo[i]->addItem("On");     // index 0
        macroRFCombo[i]->addItem("Off");    // index 1
        macroRFCombo[i]->setCurrentIndex(0);
        macroTable->setCellWidget(i, 4, macroRFCombo[i]);

        /* Col 5: PLL recalculation indicator (read-only, auto-calculated) */
        QWidget     *pllContainer = new QWidget();
        QHBoxLayout *pllLayout    = new QHBoxLayout(pllContainer);
        pllLayout->setAlignment(Qt::AlignCenter);
        pllLayout->setContentsMargins(0, 0, 0, 0);
        macroPLLCheck[i] = new QCheckBox();
        macroPLLCheck[i]->setEnabled(false);    // user cannot edit
        pllLayout->addWidget(macroPLLCheck[i]);
        macroTable->setCellWidget(i, 5, pllContainer);

        /* Connect per-row signals for auto PLL recalc and row state update */
        connect(macroActiveCheck[i], SIGNAL(stateChanged(int)),
                this, SLOT(macro_active_changed()));
        connect(macroFreqSpinBox[i], SIGNAL(valueChanged(double)),
                this, SLOT(macro_freq_changed()));
    }

    mainLayout->addWidget(macroTable);

    /* ---- Bottom row: control buttons ---- */
    QHBoxLayout *bottomRow = new QHBoxLayout();

    pushButton_macroStart = new QPushButton("Start Macro");
    pushButton_macroStart->setFont(QFont(pushButton_macroStart->font().family(), 11));
    pushButton_macroStart->setMinimumHeight(34);
    pushButton_macroStart->setEnabled(false);
    pushButton_macroStart->setToolTip("Waiting for device connection...");

    pushButton_macroStop = new QPushButton("Stop Macro");
    pushButton_macroStop->setFont(QFont(pushButton_macroStop->font().family(), 11));
    pushButton_macroStop->setMinimumHeight(34);
    pushButton_macroStop->setEnabled(false);

    checkBox_macroLoop = new QCheckBox("Loop");
    checkBox_macroLoop->setFont(QFont(checkBox_macroLoop->font().family(), 11));
    checkBox_macroLoop->setChecked(true);

    pushButton_macroProgram = new QPushButton("Program Macro to Flash");
    pushButton_macroProgram->setFont(QFont(pushButton_macroProgram->font().family(), 11));
    pushButton_macroProgram->setMinimumHeight(34);
    pushButton_macroProgram->setEnabled(false);
    pushButton_macroProgram->setToolTip("Waiting for device connection...");

    label_macroStepElapsed = new QLabel("");
    label_macroStepElapsed->setFont(QFont(label_macroStepElapsed->font().family(), 10));
    label_macroStepElapsed->setStyleSheet("color: #666;");
    label_macroStepElapsed->setVisible(false);

    bottomRow->addWidget(pushButton_macroStart);
    bottomRow->addWidget(pushButton_macroStop);
    bottomRow->addWidget(checkBox_macroLoop);
    bottomRow->addWidget(label_macroStepElapsed);
    bottomRow->addStretch();
    bottomRow->addWidget(pushButton_macroProgram);

    mainLayout->addLayout(bottomRow);

    /* ---- Connect macro buttons ---- */
    connect(pushButton_macroStart,   SIGNAL(clicked(bool)), this, SLOT(macro_start_click()));
    connect(pushButton_macroStop,    SIGNAL(clicked(bool)), this, SLOT(macro_stop_click()));
    connect(pushButton_macroProgram, SIGNAL(clicked(bool)), this, SLOT(macro_program_click()));

    /* ---- Apply initial row states and calculate PLL flags ---- */
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        updateMacroRowState(i);
    }
    recalcMacroPLL();
}


/**
 * @brief Slot called when any Active checkbox changes.
 *
 * Enforces sequential activation: checking step N auto-checks all steps 2..N;
 * unchecking step N auto-unchecks all steps N..13.  Steps 0–1 are always on.
 */
void USBIOBoard::macro_active_changed()
{
    /* Determine which checkbox the user clicked */
    QCheckBox *senderCheck = qobject_cast<QCheckBox*>(QObject::sender());
    int changedIdx = -1;
    for (int i = 2; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i] == senderCheck)
        {
            changedIdx = i;
            break;
        }
    }

    /* Enforce sequential ordering (no fragmented steps) */
    if (changedIdx >= 2)
    {
        /* Block signals to prevent recursive calls */
        for (int i = 2; i < MACRO_MAX_STEPS; i++)
            macroActiveCheck[i]->blockSignals(true);

        if (senderCheck->isChecked())
        {
            /* Checking step N: fill all steps 2..N */
            for (int i = 2; i <= changedIdx; i++)
                macroActiveCheck[i]->setChecked(true);
        }
        else
        {
            /* Unchecking step N: clear all steps N..13 */
            for (int i = changedIdx; i < MACRO_MAX_STEPS; i++)
                macroActiveCheck[i]->setChecked(false);
        }

        /* Restore signals */
        for (int i = 2; i < MACRO_MAX_STEPS; i++)
            macroActiveCheck[i]->blockSignals(false);
    }

    /* Update row enable/disable states and recalculate PLL */
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
        updateMacroRowState(i);

    recalcMacroPLL();

    /* Update active step count in status label */
    int count = 0;
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i]->isChecked())
            count++;
    }

    if (!isMacroRunning)
        label_macroStatus->setText(QString("Status: Idle  |  Active Steps: %1").arg(count));
}


/**
 * @brief Slot called when any frequency spinbox value changes.
 *        Triggers PLL recalculation across all active steps.
 */
void USBIOBoard::macro_freq_changed()
{
    recalcMacroPLL();
}


/**
 * @brief Enables or disables the editable widgets for a single table row
 *        based on its Active checkbox state.
 */
void USBIOBoard::updateMacroRowState(int row)
{
    bool active = macroActiveCheck[row]->isChecked();

    macroFreqSpinBox[row]->setEnabled(active);
    macroTimeSpinBox[row]->setEnabled(active);
    macroRFCombo[row]->setEnabled(active);
    /* PLL checkbox always disabled — it's auto-calculated */

    /* Grey out the step number for inactive rows */
    QTableWidgetItem *item = macroTable->item(row, 0);
    if (item)
        item->setForeground(active ? Qt::black : Qt::gray);
}


/**
 * @brief Auto-calculates the "Recal PLL" checkbox for each active step.
 *
 * Rules:
 *  - Compare each step's frequency with the previous active step.
 *  - First step compares with the last active step (loop wrap-around).
 *  - If frequencies differ → PLL recalc needed.
 *  - If only one step → always recalc.
 *
 * Note: The first iteration's PLL init is handled separately by
 *       macroFirstIteration flag (app-side) and firmware init state (device-side).
 */
void USBIOBoard::recalcMacroPLL()
{
    /* Collect indices of all active steps */
    QList<int> active;
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i]->isChecked())
            active.append(i);
    }

    /* Clear all PLL flags */
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
        macroPLLCheck[i]->setChecked(false);

    /* Single step: always needs PLL */
    if (active.size() <= 1)
    {
        if (active.size() == 1)
            macroPLLCheck[active[0]]->setChecked(true);
        return;
    }

    /* Compare each active step with its predecessor (with loop wrap-around) */
    for (int i = 0; i < active.size(); i++)
    {
        int prevIdx = (i == 0) ? active.last() : active[i - 1];
        int curIdx  = active[i];

        double prevFreq = macroFreqSpinBox[prevIdx]->value();
        double curFreq  = macroFreqSpinBox[curIdx]->value();

        if (qAbs(curFreq - prevFreq) > 0.005)
            macroPLLCheck[curIdx]->setChecked(true);
    }
}


/**
 * @brief Returns (highest active step index + 1), i.e. the total range
 *        that the macro execution loop and firmware macroSteps must cover.
 */
int USBIOBoard::getMacroTotalRange()
{
    for (int i = MACRO_MAX_STEPS - 1; i >= 0; i--)
    {
        if (macroActiveCheck[i]->isChecked())
            return i + 1;
    }
    return 0;
}


/**
 * @brief Packs the macro table data into two devicemacro_s blocks for
 *        flash programming via USB.
 *
 * Block 0 holds steps 0–6 (rows 1–7), block 1 holds steps 7–13 (rows 8–14).
 * macroSteps per block = (highest active step index within that block) + 1.
 * Inactive gap entries get STEP_FLAG_ENABLED=0 so firmware skips them.
 */
void USBIOBoard::packMacroData(devicemacro_s *block0, devicemacro_s *block1)
{
    memset(block0, 0, sizeof(devicemacro_s));
    memset(block1, 0, sizeof(devicemacro_s));

    /* Find highest active step index in each block */
    int highestBlock0 = -1;
    int highestBlock1 = -1;

    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i]->isChecked())
        {
            if (i < MACRO_STEPS_PER_BLOCK)
                highestBlock0 = i;
            else
                highestBlock1 = i - MACRO_STEPS_PER_BLOCK;
        }
    }

    block0->macroSteps = (highestBlock0 >= 0) ? (highestBlock0 + 1) : 0;
    block1->macroSteps = (highestBlock1 >= 0) ? (highestBlock1 + 1) : 0;

    /* Pack block 0 (steps 0–6) */
    for (int i = 0; i < (int)block0->macroSteps; i++)
    {
        /* Frequency: MHz × 100 → 0.01 MHz fixed-point for firmware */
        uint32_t freqFP = static_cast<uint32_t>(macroFreqSpinBox[i]->value() * 100.0);
        packUint24LE(block0->stepConfig[i].frequency, freqFP);

        /* Dwell time in milliseconds */
        uint32_t timeMs = static_cast<uint32_t>(macroTimeSpinBox[i]->value());
        packUint24LE(block0->stepConfig[i].step_ms, timeMs);

        /* Status flags */
        uint8_t flags = 0;

        if (macroActiveCheck[i]->isChecked())
            flags |= STEP_FLAG_ENABLED;

        /* RF output: combo index 0="On", 1="Off" */
        uint8_t rfVal = (macroRFCombo[i]->currentIndex() != 0)
                        ? STEP_FLAG_RF_ON : STEP_FLAG_RF_OFF;
        flags |= (rfVal << STEP_FLAG_RF_SHIFT);

        if (macroPLLCheck[i]->isChecked())
            flags |= STEP_FLAG_RECALC_PLL;

        block0->stepConfig[i].status_flag = flags;
    }

    /* Pack block 1 (steps 7–13) */
    for (int i = 0; i < (int)block1->macroSteps; i++)
    {
        int uiRow = i + MACRO_STEPS_PER_BLOCK;

        uint32_t freqFP = static_cast<uint32_t>(macroFreqSpinBox[uiRow]->value() * 100.0);
        packUint24LE(block1->stepConfig[i].frequency, freqFP);

        uint32_t timeMs = static_cast<uint32_t>(macroTimeSpinBox[uiRow]->value());
        packUint24LE(block1->stepConfig[i].step_ms, timeMs);

        uint8_t flags = 0;

        if (macroActiveCheck[uiRow]->isChecked())
            flags |= STEP_FLAG_ENABLED;

        uint8_t rfVal = (macroRFCombo[uiRow]->currentIndex() != 0)
                        ? STEP_FLAG_RF_ON : STEP_FLAG_RF_OFF;
        flags |= (rfVal << STEP_FLAG_RF_SHIFT);

        if (macroPLLCheck[uiRow]->isChecked())
            flags |= STEP_FLAG_RECALC_PLL;

        block1->stepConfig[i].status_flag = flags;
    }
}


/**
 * @brief Packs macro data and triggers USB flash write to device.
 */
void USBIOBoard::macro_program_click()
{
    if (!this->isDeviceConnected)
    {
        label_macroStatus->setText("Status: No Device");
        return;
    }

    devicemacro_s block0, block1;
    packMacroData(&block0, &block1);

    this->macroBlock0 = block0;
    this->macroBlock1 = block1;
    this->isMacroProgramPending = true;
}


/**
 * @brief Starts app-side macro execution.
 *
 * Disables main controls (Write, RF, Sweep) to prevent conflicts.
 * Shows an orange dot on the Macro tab to indicate running state.
 * Forces auto_tx on so register changes are sent immediately.
 */
void USBIOBoard::macro_start_click()
{
    if (getMacroTotalRange() < 2)
        return;

    if (!this->isDeviceConnected)
    {
        label_macroStatus->setText("Status: No Device");
        return;
    }

    /* Stop any running sweep (clears orange dot + tooltips) */
    if (sweep_timer->isActive())
        sweep_stop_click();

    /* Save auto-write state, force it on, and disable checkbox */
    enable_auto_tx_before_macro = ui->checkBox_autotx->isChecked();
    this->enable_auto_tx = true;
    ui->checkBox_autotx->setChecked(true);
    ui->checkBox_autotx->setEnabled(false);
    ui->checkBox_autotx->setToolTip("Auto write is forced on during macro");

    /* Set execution state */
    isMacroRunning      = true;
    currentMacroStep    = 0;
    macroFirstIteration = true;

    /* Disable macro tab controls */
    pushButton_macroStart->setEnabled(false);
    pushButton_macroStop->setEnabled(true);
    pushButton_macroProgram->setEnabled(false);

    for (int i = 0; i < MACRO_MAX_STEPS; i++)
        macroActiveCheck[i]->setEnabled(false);

    /* Disable main controls to prevent conflicts */
    ui->USBTX->setEnabled(false);
    ui->USBTX->setToolTip("Not available while macro is running");
    ui->RF_CTRL->setEnabled(false);
    ui->RF_CTRL->setToolTip("Not available while macro is running");
    ui->pushButton_sweep_start->setEnabled(false);
    ui->pushButton_sweep_start->setToolTip("Not available while macro is running");
    ui->pushButton_sweep_stop->setEnabled(false);
    ui->pushButton_sweep_stop->setToolTip("Not available while macro is running");
    ui->pushButton_program_flash->setEnabled(false);
    ui->pushButton_program_flash->setToolTip("Not available while macro is running");
    ui->pushButton_erase_flash->setEnabled(false);
    ui->pushButton_erase_flash->setToolTip("Not available while macro is running");

    /* Show orange dot on Macro tab header */
    QPixmap dot(10, 10);
    dot.fill(Qt::transparent);
    QPainter painter(&dot);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(255, 165, 0));  // orange
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 10, 10);
    painter.end();

    int tabIdx = ui->tabWidget->indexOf(ui->tabWidget->findChild<QWidget*>("tab"));
    if (tabIdx >= 0)
        ui->tabWidget->setTabIcon(tabIdx, QIcon(dot));

    label_macroStatus->setText("Status: Running");

    /* Execute first step immediately */
    executeMacroStep();
}


/**
 * @brief Stops app-side macro execution and restores all controls.
 */
void USBIOBoard::macro_stop_click()
{
    macro_timer->stop();
    macro_elapsed_timer->stop();
    label_macroStepElapsed->setVisible(false);
    isMacroRunning = false;

    /* Restore auto-write checkbox to saved state */
    ui->checkBox_autotx->setEnabled(true);
    ui->checkBox_autotx->setChecked(enable_auto_tx_before_macro);
    ui->checkBox_autotx->setToolTip("Automatically send register updates on any parameter change");
    this->enable_auto_tx = enable_auto_tx_before_macro;

    /* Re-enable macro tab controls */
    pushButton_macroStart->setEnabled(true);
    pushButton_macroStop->setEnabled(false);

    /* Only re-enable Program to Flash if firmware supports it */
    if (this->deviceFirmwareMajor >= 2)
    {
        pushButton_macroProgram->setEnabled(true);
        pushButton_macroProgram->setToolTip("Write macro sequence to device flash (requires firmware 2.0+)");
    }
    else
    {
        pushButton_macroProgram->setEnabled(false);
        pushButton_macroProgram->setToolTip("Only supported with Device Firmware 2.0+\nPlease update firmware.");
    }

    for (int i = 0; i < MACRO_MAX_STEPS; i++)
        macroActiveCheck[i]->setEnabled(i >= 2);   // first two stay locked

    /* Re-enable main controls */
    ui->USBTX->setEnabled(!ui->checkBox_autotx->isChecked());
    ui->USBTX->setToolTip("Manually send current register values to device via USB");
    ui->RF_CTRL->setEnabled(true);
    ui->RF_CTRL->setToolTip("Toggle RF output on / off");
    ui->pushButton_sweep_start->setEnabled(true);
    ui->pushButton_sweep_start->setToolTip("Start frequency sweep");
    ui->pushButton_sweep_stop->setEnabled(true);
    ui->pushButton_sweep_stop->setToolTip("Stop frequency sweep");
    ui->pushButton_erase_flash->setEnabled(true);
    ui->pushButton_erase_flash->setToolTip("Erase all saved settings from device flash");

    /* Program flash: re-enable only if firmware supports it */
    /* (update_gui will set correct state on next tick) */

    /* Remove orange dot from Macro tab header */
    int tabIdx = ui->tabWidget->indexOf(ui->tabWidget->findChild<QWidget*>("tab"));
    if (tabIdx >= 0)
        ui->tabWidget->setTabIcon(tabIdx, QIcon());

    /* Update status with active step count */
    int count = 0;
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i]->isChecked())
            count++;
    }
    label_macroStatus->setText(QString("Status: Idle  |  Active Steps: %1").arg(count));
    clearMacroHighlight();
}


/**
 * @brief Timer callback — advances to the next macro step.
 */
void USBIOBoard::macro_timer_timeout()
{
    if (!isMacroRunning)
        return;

    currentMacroStep++;
    executeMacroStep();
}


/**
 * @brief Updates the elapsed time display for the current macro step.
 *        Only active for steps longer than 5000 ms.
 */
void USBIOBoard::macro_elapsed_timer_timeout()
{
    if (!isMacroRunning)
    {
        macro_elapsed_timer->stop();
        label_macroStepElapsed->setVisible(false);
        return;
    }

    int stepIdx = currentMacroStep % MACRO_MAX_STEPS;
    int totalMs = macroTimeSpinBox[stepIdx]->value();
    qint64 elapsedMs = macroStepElapsed.elapsed();

    double elapsedSec = elapsedMs / 1000.0;
    double totalSec   = totalMs / 1000.0;

    label_macroStepElapsed->setText(
        QString("%1s / %2s")
            .arg(elapsedSec, 0, 'f', 1)
            .arg(totalSec, 0, 'f', 1));
}


/**
 * @brief Executes the current macro step: sets RF state, optionally
 *        recalculates PLL, and starts the dwell timer.
 *
 * Skips inactive steps.  On the very first step of the macro
 * (macroFirstIteration), PLL recalc is forced regardless of the
 * auto-calculated flag to ensure the PLL is properly initialised.
 */
void USBIOBoard::executeMacroStep()
{
    int totalRange = getMacroTotalRange();

    if (totalRange == 0 || !isMacroRunning)
    {
        macro_stop_click();
        return;
    }

    /* Find the next active step, skipping inactive ones */
    int attempts = 0;
    while (attempts < MACRO_MAX_STEPS)
    {
        if (currentMacroStep >= totalRange)
        {
            if (checkBox_macroLoop->isChecked())
                currentMacroStep = 0;
            else
            {
                macro_stop_click();
                return;
            }
        }

        if (macroActiveCheck[currentMacroStep]->isChecked())
            break;

        currentMacroStep++;
        attempts++;
    }

    if (attempts >= MACRO_MAX_STEPS)
    {
        macro_stop_click();     // no active steps found
        return;
    }

    /* Highlight the current step row in the table */
    highlightMacroRow(currentMacroStep);

    int stepIdx = currentMacroStep;

    /* Update status label */
    int activeCount = 0;
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        if (macroActiveCheck[i]->isChecked())
            activeCount++;
    }
    label_macroStatus->setText(
        QString("Status: Step %1  |  Active Steps: %2")
            .arg(stepIdx + 1)
            .arg(activeCount)
    );

    /* Set RF output state: combo index 0="On"→true, 1="Off"→false */
    bool rfOn = (macroRFCombo[stepIdx]->currentIndex() == 0);
    emit signal_set_RF_state(rfOn);
    this->lastKnownRFOn = rfOn;
    updateDeviceDiagram(rfOn);

    /* PLL recalculation:
     *  - Always on first iteration (PLL state unknown at macro start)
     *  - Otherwise follow the auto-calculated PLL checkbox */
    bool needPLL = macroPLLCheck[stepIdx]->isChecked() || macroFirstIteration;
    macroFirstIteration = false;

    if (needPLL)
    {
        double freq = macroFreqSpinBox[stepIdx]->value();
        this->adf4351->frequency = freq;

        /* Update main frequency displays without triggering recalculate() */
        ui->doubleSpinBox_freq_basic->blockSignals(true);
        ui->doubleSpinBox_freq_basic->setValue(freq);
        ui->doubleSpinBox_freq_basic->blockSignals(false);

        ui->doubleSpinBox_freq->blockSignals(true);
        ui->doubleSpinBox_freq->setValue(freq);
        ui->doubleSpinBox_freq->blockSignals(false);

        /* Build registers and send to device */
        emit singal_recalculate();
    }

    /* Start dwell timer for this step */
    int stepDuration = macroTimeSpinBox[stepIdx]->value();
    macro_timer->start(stepDuration);

    /* Show elapsed counter */
    macroStepElapsed.start();
    macro_elapsed_timer->start(1000);
    label_macroStepElapsed->setVisible(true);
    label_macroStepElapsed->setText(
        QString("0.0s / %1s").arg(stepDuration / 1000.0, 0, 'f', 1));
}


/**
 * @brief Highlights the given row's step number cell with a green background.
 */
void USBIOBoard::highlightMacroRow(int row)
{
    clearMacroHighlight();

    QTableWidgetItem *item = macroTable->item(row, 0);
    if (item)
        item->setBackground(QColor(0x40, 0xC0, 0x40, 0x80));   // light green
}


/**
 * @brief Clears the green highlight from all step number cells.
 */
void USBIOBoard::clearMacroHighlight()
{
    for (int r = 0; r < MACRO_MAX_STEPS; r++)
    {
        QTableWidgetItem *item = macroTable->item(r, 0);
        if (item)
            item->setBackground(QBrush());
    }
}


/*******************************************************************************
 *
 *  ORIGINAL APPLICATION LOGIC — Sweep, register I/O, USB device management
 *
 ******************************************************************************/

void USBIOBoard::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    this->recalculate();
}


USBIOBoard::~USBIOBoard()
{
    disconnect(plugNPlay, SIGNAL(hid_comm_update(bool, UI_Data)),
               this, SLOT(update_gui(bool, UI_Data)));
    delete ui;
    delete plugNPlay;
}


void USBIOBoard::update_dac()
{
    // placeholder — DAC not used in this product
}


/**
 * @brief Reads all ADF4351 parameter values from the UI widgets
 *        into the adf4351 object.
 */
void USBIOBoard::getDataFromUI()
{
    /* Sync basic/advanced frequency displays */
    if (ui->tabWidget->currentIndex() == 0)
        ui->doubleSpinBox_freq->setValue(ui->doubleSpinBox_freq_basic->value());
    else
        ui->doubleSpinBox_freq_basic->setValue(ui->doubleSpinBox_freq->value());

    /* Core parameters */
    this->adf4351->frequency    = ui->doubleSpinBox_freq->value();
    this->adf4351->start_freq   = ui->doubleSpinBox_sweep_start->value();
    this->adf4351->stop_freq    = ui->doubleSpinBox_sweep_end->value();
    this->adf4351->step_freq    = ui->doubleSpinBox_sweep_step->value();
    this->adf4351->step_ms      = ui->spinBox_sweep_delay->value();
    this->adf4351->aux_select   = static_cast<uint16_t>(ui->comboBox_auxsetting->currentIndex());
    this->adf4351->isStartOnBoot  = ui->checkBox_autoLockOnBoot->isChecked();
    this->adf4351->isSweepEnabled = ui->checkBox_prog_enable_sweep->isChecked();

    /* Reference and divider settings */
    this->adf4351->ref_freq    = ui->doubleSpinBox_ref->value();
    this->adf4351->r_counter   = ui->spinBox_rcount->text().toInt();
    this->adf4351->PHASE       = ui->spinBox_phase_val->text().toInt();
    this->adf4351->PHASE_ADJUST = ui->comboBox_phase_adjust->currentIndex();
    this->adf4351->ref_div2    = ui->checkBox_refdiv2->isChecked();
    this->adf4351->ref_doubler = ui->checkBox_refx2->isChecked();

    /* Register configuration */
    this->adf4351->low_noise_spur_mode     = ui->comboBox_mode->currentIndex();
    this->adf4351->muxout                  = ui->comboBox_mux_out->currentIndex();
    this->adf4351->double_buff             = ui->comboBox_double_buff->currentIndex();
    this->adf4351->charge_pump_current     = ui->comboBox_charge_pump_current->currentIndex();
    this->adf4351->LDF                     = ui->comboBox_LDF->currentIndex();
    this->adf4351->LDP                     = ui->comboBox_LDP->currentIndex();
    this->adf4351->PD_Polarity             = ui->comboBox_PD_polarity->currentIndex();
    this->adf4351->cp_3stage               = ui->comboBox_cp_3_state->currentIndex();
    this->adf4351->counter_reset           = ui->comboBox_counter_rst->currentIndex();
    this->adf4351->band_select_clock_mode  = ui->comboBox_band_select_clk_mode->currentIndex();
    this->adf4351->charge_cancelletion     = ui->comboBox_charge_cancellation->currentIndex();
    this->adf4351->ABP                     = ui->comboBox_ABP->currentIndex();
    this->adf4351->CSR                     = ui->comboBox_CSR->currentIndex();
    this->adf4351->clock_divider           = ui->spinBox_clock_divider->value();
    this->adf4351->CLK_DIV_MODE            = ui->comboBox_CLK_div_mode->currentIndex();
    this->adf4351->LD                      = ui->comboBox_LDPIN->currentIndex();
    this->adf4351->power_down              = ui->comboBox_powerdown->currentIndex();
    this->adf4351->VCO_power_down          = ui->comboBox_vco_powerdown->currentIndex();
    this->adf4351->MTLD                    = ui->comboBox_MTLD->currentIndex();
    this->adf4351->AUX_output_mode         = ui->comboBox_AUX_OUT->currentIndex();
    this->adf4351->AUX_output_enable       = ui->comboBox_AUX_EN->currentIndex();
    this->adf4351->AUX_output_power        = ui->comboBox_AUX_out_power->currentIndex();
    this->adf4351->RF_output_power         = ui->comboBox_RF_POWER->currentIndex();
    this->adf4351->RF_OUT                  = ui->comboBox_RF_OUT->currentIndex();
    this->adf4351->PR1                     = ui->comboBox_presacler->currentIndex();
    this->adf4351->feedback_select         = ui->comboBox_feedback->currentIndex();

    /* Constrain sweep step range */
    ui->doubleSpinBox_sweep_end->setMinimum(ui->doubleSpinBox_sweep_start->value());

    double maxStep = qMin(
        ui->doubleSpinBox_sweep_end->value() - ui->doubleSpinBox_sweep_start->value(),
        4400.00
    );
    if (maxStep < 0.0)
        maxStep = 0;

    ui->doubleSpinBox_sweep_step->setMaximum(maxStep);

    if (ui->doubleSpinBox_sweep_step->value() == 0.0 && maxStep != 0.0)
        ui->doubleSpinBox_sweep_step->setValue(1.0);
}


void USBIOBoard::recalculate()
{
    this->getDataFromUI();
    updateSweepInfo();
    emit singal_recalculate();
}


/**
 * @brief Updates the sweep info label with calculated step count,
 *        max step size, and total sweep time.
 */
void USBIOBoard::updateSweepInfo()
{
    double startFreq = ui->doubleSpinBox_sweep_start->value();
    double stopFreq  = ui->doubleSpinBox_sweep_end->value();
    double stepFreq  = ui->doubleSpinBox_sweep_step->value();
    int    stepMs    = ui->spinBox_sweep_delay->value();

    double range = stopFreq - startFreq;

    if (range <= 0.0 || stepFreq <= 0.0)
    {
        ui->label_sweepInfo->setText("Steps: --\nMax Step: --\nTotal: --");
        return;
    }

    int numSteps = static_cast<int>(qFloor(range / stepFreq)) + 1;
    double maxStep = qMin(range, 4400.0);
    double totalTimeMs = static_cast<double>(numSteps) * stepMs;

    /* Format total time nicely */
    QString totalStr;
    if (totalTimeMs < 1000.0)
        totalStr = QString("%1 ms").arg(static_cast<int>(totalTimeMs));
    else if (totalTimeMs < 60000.0)
        totalStr = QString("%1 s").arg(totalTimeMs / 1000.0, 0, 'f', 1);
    else if (totalTimeMs < 3600000.0)
        totalStr = QString("%1 min").arg(totalTimeMs / 60000.0, 0, 'f', 1);
    else
        totalStr = QString("%1 hr").arg(totalTimeMs / 3600000.0, 0, 'f', 2);

    ui->label_sweepInfo->setText(
        QString("Steps: %1\nMax Step: %2 MHz\nTotal: %3")
            .arg(numSteps)
            .arg(maxStep, 0, 'f', 2)
            .arg(totalStr)
    );
}


/*******************************************************************************
 *  Adaptive spinbox step size — step matches cursor digit position
 ******************************************************************************/

/**
 * @brief Installs event filter and cursor tracking on all frequency/time spinboxes.
 */
void USBIOBoard::setupAdaptiveSteps()
{
    /* Collect all spinboxes that should have adaptive stepping */
    QList<QAbstractSpinBox*> spinBoxes;

    /* Control tab */
    spinBoxes << ui->doubleSpinBox_freq_basic
              << ui->doubleSpinBox_sweep_start
              << ui->doubleSpinBox_sweep_end
              << ui->doubleSpinBox_sweep_step
              << ui->spinBox_sweep_delay;

    /* Advance tab */
    spinBoxes << ui->doubleSpinBox_freq
              << ui->doubleSpinBox_ref;

    /* Macro tab */
    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        spinBoxes << macroFreqSpinBox[i];
        spinBoxes << macroTimeSpinBox[i];
    }

    /* Install event filter on each spinbox's internal QLineEdit.
     * We track mouse clicks and arrow keys — NOT cursorPositionChanged,
     * because value changes also reposition the cursor and would
     * cause the step to escalate to max. */
    for (QAbstractSpinBox *sb : spinBoxes)
    {
        QLineEdit *le = sb->findChild<QLineEdit*>();
        if (le)
            le->installEventFilter(this);
    }
}


/**
 * @brief Slot kept for compatibility — now called from eventFilter only.
 */
void USBIOBoard::onSpinBoxCursorMoved()
{
    /* Not connected to any signal — step updates happen via eventFilter */
}


/**
 * @brief Calculates and applies adaptive step size from cursor position.
 */
static void applyAdaptiveStep(QLineEdit *le)
{
    if (!le) return;

    QDoubleSpinBox *dsp = qobject_cast<QDoubleSpinBox*>(le->parent());
    QSpinBox       *sp  = qobject_cast<QSpinBox*>(le->parent());

    int cursor = le->cursorPosition();

    if (dsp)
    {
        QString prefix  = dsp->prefix();
        QString suffix  = dsp->suffix();
        QString text    = le->text();
        QString numText = text.mid(prefix.length(),
                                   text.length() - prefix.length() - suffix.length());
        int numCursor   = cursor - prefix.length();

        /* Clamp: cursor before first digit → treat as first digit */
        if (numCursor <= 0) numCursor = 1;
        if (numCursor > numText.length()) numCursor = numText.length();

        int decPos = numText.indexOf('.');
        if (decPos < 0) decPos = numText.length();

        /* Step matches the digit to the LEFT of cursor:
         *  "1234.56"  decPos=4
         *  cursor 1 → left digit '1' (thousands) → step 1000
         *  cursor 2 → left digit '2' (hundreds)  → step 100
         *  cursor 3 → left digit '3' (tens)      → step 10
         *  cursor 4 → left digit '4' (ones)      → step 1
         *  cursor 5 → left is '.' (no digit)     → step 1
         *  cursor 6 → left digit '5' (tenths)    → step 0.1
         *  cursor 7 → left digit '6' (hundredths)→ step 0.01 */
        double step;
        if (numCursor <= decPos)
            step = qPow(10.0, decPos - numCursor);
        else if (numCursor == decPos + 1)
            step = 1.0;
        else
            step = qPow(10.0, decPos - numCursor + 1);

        if (step < 0.01) step = 0.01;
        if (step > 10000.0) step = 10000.0;

        dsp->setSingleStep(step);
    }
    else if (sp)
    {
        QString prefix  = sp->prefix();
        QString suffix  = sp->suffix();
        QString text    = le->text();
        QString numText = text.mid(prefix.length(),
                                   text.length() - prefix.length() - suffix.length());
        int numCursor   = cursor - prefix.length();

        if (numCursor <= 0) numCursor = 1;
        if (numCursor > numText.length()) numCursor = numText.length();

        /* Step = place value of digit to the LEFT of cursor */
        int step = static_cast<int>(qPow(10.0, numText.length() - numCursor));
        if (step < 1) step = 1;

        sp->setSingleStep(step);
    }
}


/**
 * @brief Event filter — updates spinbox step only on explicit user
 *        cursor moves (mouse click, arrow keys), NOT on value-change
 *        triggered cursor repositioning.
 */
bool USBIOBoard::eventFilter(QObject *obj, QEvent *event)
{
    QLineEdit *le = qobject_cast<QLineEdit*>(obj);
    if (le)
    {
        if (event->type() == QEvent::MouseButtonRelease)
        {
            applyAdaptiveStep(le);
        }
        else if (event->type() == QEvent::KeyRelease)
        {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            int key = ke->key();
            if (key == Qt::Key_Left  || key == Qt::Key_Right ||
                key == Qt::Key_Home  || key == Qt::Key_End)
            {
                applyAdaptiveStep(le);
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}


/**
 * @brief Reads hex register values from the UI line edits and sends
 *        them to the device via USB.
 */
/**
 * @brief Sets descriptive tooltips on every interactive UI component.
 */
void USBIOBoard::setupTooltips()
{
    /* ---- Control tab: Frequency group ---- */
    ui->doubleSpinBox_freq_basic->setToolTip("Set RF output frequency (35 – 4400 MHz)\nClick on a digit to set step size");
    ui->comboBox_RF_POWER_basic->setToolTip("Set RF output power level");
    ui->label_deviceDiagram->setToolTip("Device block diagram — click RF OUT to toggle output on/off");

    /* ---- Control tab: Sweep / Hop group ---- */
    ui->doubleSpinBox_sweep_start->setToolTip("Sweep start frequency (MHz)");
    ui->doubleSpinBox_sweep_end->setToolTip("Sweep end frequency (MHz)");
    ui->doubleSpinBox_sweep_step->setToolTip("Frequency step per sweep increment (MHz)");
    ui->spinBox_sweep_delay->setToolTip("Time delay between each sweep step (ms)");
    ui->checkBox_sweep_loop->setToolTip("Continuously loop sweep from start to end");
    ui->pushButton_sweep_start->setToolTip("Start frequency sweep");
    ui->pushButton_sweep_stop->setToolTip("Stop frequency sweep");

    /* ---- Control tab: Device Settings group ---- */
    ui->comboBox_auxsetting->setToolTip("Configure AUX pin:\n  Sync Out – output sync pulse\n  Sync In – accept external sync\n  Ext Ref EN – use external reference clock");
    ui->checkBox_autoLockOnBoot->setToolTip("Load saved settings from flash on device power-up");
    ui->checkBox_prog_enable_sweep->setToolTip("Enable device-side sweep using flash settings");
    ui->pushButton_erase_flash->setToolTip("Erase all saved settings from device flash");
    ui->pushButton_program_flash->setToolTip("Save current settings to device flash");
    ui->spinBox_serial->setToolTip("Device serial number (hexadecimal)");
    ui->pushButton_program_serial->setToolTip("Write serial number to device");

    /* ---- Bottom bar ---- */
    ui->checkBox_autotx->setToolTip("Automatically send register updates on any parameter change");
    ui->USBTX->setToolTip("Manually send current register values to device via USB");
    ui->RF_CTRL->setToolTip("Toggle RF output on / off");
    ui->comboBox_usb_devices->setToolTip("Select which USB device to connect to");
    ui->pushButton_identify->setToolTip("Blink LED on the selected device for identification");

    /* ---- Advance Control tab ---- */
    ui->doubleSpinBox_freq->setToolTip("RF output frequency (35 – 4400 MHz)");
    ui->checkBox_refx2->setToolTip("Reference doubler – doubles the reference frequency");
    ui->checkBox_refdiv2->setToolTip("Reference divide-by-2 – halves the reference frequency");
    ui->doubleSpinBox_ref->setToolTip("Reference oscillator frequency (MHz)");
    ui->spinBox_rcount->setToolTip("R counter divider value");
    ui->spinBox_clock_divider->setToolTip("Clock divider value");
    ui->spinBox_phase_val->setToolTip("Phase value (0 – 4095)");
    ui->comboBox_mux_out->setToolTip("Multiplexer output pin function");
    ui->comboBox_PD_polarity->setToolTip("Phase detector polarity");
    ui->comboBox_powerdown->setToolTip("Power-down mode");
    ui->comboBox_cp_3_state->setToolTip("Charge pump three-state mode");
    ui->comboBox_counter_rst->setToolTip("Counter reset");
    ui->comboBox_vco_powerdown->setToolTip("VCO power-down");
    ui->comboBox_RF_OUT->setToolTip("RF output enable / disable");
    ui->comboBox_ABP->setToolTip("Anti-backlash pulse width");
    ui->comboBox_CSR->setToolTip("Cycle slip reduction");
    ui->comboBox_LDP->setToolTip("Lock detect precision");
    ui->comboBox_MTLD->setToolTip("Mute till lock detect");
    ui->comboBox_AUX_OUT->setToolTip("Auxiliary output enable / disable");
    ui->comboBox_AUX_EN->setToolTip("Auxiliary output power enable");
    ui->comboBox_AUX_out_power->setToolTip("Auxiliary output power level");
    ui->comboBox_presacler->setToolTip("Prescaler (4/5 or 8/9)");
    ui->comboBox_RF_POWER->setToolTip("RF output power level");
    ui->comboBox_phase_adjust->setToolTip("Phase adjust enable");
    ui->comboBox_CLK_div_mode->setToolTip("Clock divider mode");
    ui->comboBox_LDPIN->setToolTip("Lock detect pin function");
    ui->comboBox_charge_pump_current->setToolTip("Charge pump current setting");
    ui->comboBox_LDF->setToolTip("Lock detect function (integer / fractional)");
    ui->comboBox_mode->setToolTip("Low noise or low spur mode");
    ui->comboBox_feedback->setToolTip("VCO feedback path (fundamental / divided)");
    ui->comboBox_band_select_clk_mode->setToolTip("Band select clock mode");
    ui->comboBox_charge_cancellation->setToolTip("Charge cancellation enable");
    ui->comboBox_double_buff->setToolTip("Double buffer enable for Register 4");

    /* ---- Advance Control tab: Register hex values ---- */
    ui->line_reg0->setToolTip("Register 0 hex value (edit to write directly)");
    ui->line_reg1->setToolTip("Register 1 hex value");
    ui->line_reg2->setToolTip("Register 2 hex value");
    ui->line_reg3->setToolTip("Register 3 hex value");
    ui->line_reg4->setToolTip("Register 4 hex value");
    ui->line_reg5->setToolTip("Register 5 hex value");

    /* ---- Macro tab ---- */
    pushButton_macroStart->setToolTip("Start executing macro sequence from step 1");
    pushButton_macroStop->setToolTip("Stop macro execution");
    pushButton_macroProgram->setToolTip("Write macro sequence to device flash (requires firmware 2.0+)");
    checkBox_macroLoop->setToolTip("Continuously loop the macro sequence");
    label_macroStepElapsed->setToolTip("Elapsed time / total duration of current macro step\n(shown for steps longer than 5 seconds)");

    for (int i = 0; i < MACRO_MAX_STEPS; i++)
    {
        QString stepStr = QString("Step %1").arg(i + 1);
        macroFreqSpinBox[i]->setToolTip(stepStr + ": RF frequency (MHz)\nClick on a digit to set step size");
        macroTimeSpinBox[i]->setToolTip(stepStr + ": Duration (ms)\nClick on a digit to set step size");
        macroRFCombo[i]->setToolTip(stepStr + ": RF output On / Off");
        macroPLLCheck[i]->setToolTip(stepStr + ": Recalculate PLL registers (auto-calculated)");
        macroActiveCheck[i]->setToolTip(stepStr + ": Enable or disable this step");
    }
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


void USBIOBoard::onDiagramLinkClicked(const QString &link)
{
    if (link == "toggle_rf" && this->isDeviceConnected)
        emit signal_update_RF_CTRL();
}


/*******************************************************************************
 *  Sweep
 ******************************************************************************/

void USBIOBoard::sweep_stop_click()
{
    sweep_timer->stop();

    /* Restore auto-write checkbox to saved state */
    ui->checkBox_autotx->setEnabled(true);
    ui->checkBox_autotx->setChecked(this->enable_auto_tx_before_sweep);
    ui->checkBox_autotx->setToolTip("Automatically send register updates on any parameter change");
    this->enable_auto_tx = this->enable_auto_tx_before_sweep;

    /* Re-enable controls disabled during sweep */
    ui->USBTX->setEnabled(!ui->checkBox_autotx->isChecked());
    ui->USBTX->setToolTip("Manually send current register values to device via USB");
    ui->pushButton_sweep_start->setEnabled(true);
    ui->pushButton_sweep_start->setToolTip("Start frequency sweep");
    ui->pushButton_erase_flash->setEnabled(true);
    ui->pushButton_erase_flash->setToolTip("Erase all saved settings from device flash");

    pushButton_macroStart->setEnabled(true);
    pushButton_macroStart->setToolTip("Start executing macro sequence from step 1");

    /* Only re-enable Program to Flash if firmware supports it */
    if (this->deviceFirmwareMajor >= 2)
    {
        pushButton_macroProgram->setEnabled(true);
        pushButton_macroProgram->setToolTip("Write macro sequence to device flash (requires firmware 2.0+)");
    }
    else
    {
        pushButton_macroProgram->setEnabled(false);
        pushButton_macroProgram->setToolTip("Only supported with Device Firmware 2.0+\nPlease update firmware.");
    }

    /* Program flash: re-enable only if firmware supports it */
    /* (update_gui will set correct state on next tick) */

    /* Remove orange dot from Control tab */
    int tabIdx = ui->tabWidget->indexOf(ui->tab_4);
    if (tabIdx >= 0)
        ui->tabWidget->setTabIcon(tabIdx, QIcon());
}


void USBIOBoard::sweep_start_click()
{
    /* Stop macro if running */
    if (isMacroRunning)
        macro_stop_click();

    /* Save auto-write state, force it on, and disable checkbox */
    this->enable_auto_tx_before_sweep = ui->checkBox_autotx->isChecked();
    this->enable_auto_tx = true;
    ui->checkBox_autotx->setChecked(true);
    ui->checkBox_autotx->setEnabled(false);
    ui->checkBox_autotx->setToolTip("Auto write is forced on during sweep");

    /* Show start frequency on main display */
    ui->doubleSpinBox_freq_basic->blockSignals(true);
    ui->doubleSpinBox_freq_basic->setValue(ui->doubleSpinBox_sweep_start->value());
    ui->doubleSpinBox_freq_basic->blockSignals(false);

    /* Disable controls that conflict with sweep */
    ui->USBTX->setEnabled(false);
    ui->USBTX->setToolTip("Not available while sweep is active");
    ui->pushButton_program_flash->setEnabled(false);
    ui->pushButton_program_flash->setToolTip("Not available while sweep is active");
    ui->pushButton_erase_flash->setEnabled(false);
    ui->pushButton_erase_flash->setToolTip("Not available while sweep is active");

    pushButton_macroStart->setEnabled(false);
    pushButton_macroStart->setToolTip("Not available while sweep is active");
    pushButton_macroProgram->setEnabled(false);
    pushButton_macroProgram->setToolTip("Not available while sweep is active");

    /* Show orange dot on Control tab header */
    QPixmap dot(10, 10);
    dot.fill(Qt::transparent);
    QPainter painter(&dot);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(255, 165, 0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 10, 10);
    painter.end();

    int tabIdx = ui->tabWidget->indexOf(ui->tab_4);
    if (tabIdx >= 0)
        ui->tabWidget->setTabIcon(tabIdx, QIcon(dot));

    /* Enable RF output and update diagram */
    emit signal_set_RF_state(true);
    this->lastKnownRFOn = true;
    updateDeviceDiagram(true);

    sweep_timer->start(ui->spinBox_sweep_delay->value());
}


void USBIOBoard::sweep_timer_timeout()
{
    double frequency = ui->doubleSpinBox_freq_basic->value()
                     + ui->doubleSpinBox_sweep_step->value();

    ui->doubleSpinBox_freq_basic->blockSignals(true);
    ui->doubleSpinBox_freq_basic->setValue(frequency);
    ui->doubleSpinBox_freq_basic->blockSignals(false);

    this->adf4351->frequency = ui->doubleSpinBox_freq_basic->value();
    this->adf4351->isStartOfSweep = false;

    if (frequency > ui->doubleSpinBox_sweep_end->value())
    {
        if (ui->checkBox_sweep_loop->isChecked())
        {
            ui->doubleSpinBox_freq_basic->blockSignals(true);
            ui->doubleSpinBox_freq_basic->setValue(ui->doubleSpinBox_sweep_start->value());
            ui->doubleSpinBox_freq_basic->blockSignals(false);
            this->adf4351->isStartOfSweep = true;
        }
        else
        {
            this->sweep_stop_click();
        }
    }

    emit singal_recalculate();
}


/*******************************************************************************
 *  Register display
 ******************************************************************************/

/**
 * @brief Updates the device state pictogram in the Frequency groupbox.
 *        Shows RF output state, reference source, and AUX/sync function.
 */
void USBIOBoard::updateDeviceDiagram(bool rfOn)
{
    int auxIdx = ui->comboBox_auxsetting->currentIndex();

    /* Reference source */
    QString refText;
    if (auxIdx == 2)
        refText = QString("EXT REF<br/>%1 MHz").arg(ui->doubleSpinBox_ref->value(), 0, 'f', 2);
    else
        refText = QString("INT REF<br/>%1 MHz").arg(ui->doubleSpinBox_ref->value(), 0, 'f', 2);

    /* AUX / Sync function with directional arrow */
    QString syncText;
    QString syncArrow;
    if (auxIdx == 0)
    {
        syncArrow = "&darr;";   /* arrow down = signal going OUT from PLL */
        syncText  = "SYNC OUT";
    }
    else if (auxIdx == 1)
    {
        syncArrow = "&uarr;";   /* arrow up = signal coming IN to PLL */
        syncText  = "SYNC IN";
    }
    else
    {
        syncArrow = "&uarr;";   /* arrow up = external ref coming IN */
        syncText  = "EXT REF";
    }

    /* RF output state colors */
    QString rfColor   = rfOn ? "#28a745" : "#dc3545";
    QString rfBg      = rfOn ? "#d4edda" : "#f8d7da";
    QString rfLabel   = rfOn ? "ON" : "OFF";

    QString html = QString(
        "<table cellspacing='0' cellpadding='6' style='border-collapse:collapse; font-size:9pt;'>"
        "<tr>"
        "  <td valign='middle' style='border:1px solid #aaa; background:#f0f0f0; padding:8px 12px;'>"
        "    <center>%1</center></td>"
        "  <td valign='middle' style='padding:4px;'><center><span style='font-size:12pt;'>&rarr;</span></center></td>"
        "  <td valign='middle' style='border:1px solid #aaa; background:#f0f0f0; padding:8px 12px;'>"
        "    <center><b>ADF4351</b><br/>PLL</center></td>"
        "  <td valign='middle' style='padding:4px;'><center><span style='font-size:12pt;'>&rarr;</span></center></td>"
        "  <td valign='middle' style='border:1px solid %2; background:%3; padding:8px 12px; cursor:pointer;'>"
        "    <center><a href='toggle_rf' style='text-decoration:none; color:inherit;'>"
        "    <b>RF OUT</b><br/><span style='color:%2;'>%4</span></a></center></td>"
        "</tr>"
        "</table>"
        "<div style='text-align:center; margin-top:2px;'>"
        "  <span style='font-size:11pt;'>%5</span><br/>"
        "  <span style='border:1px solid #aaa; background:#f0f0f0; padding:3px 12px; font-size:9pt;'>%6</span>"
        "</div>"
    ).arg(refText, rfColor, rfBg, rfLabel, syncArrow, syncText);

    ui->label_deviceDiagram->setText(html);
}


void USBIOBoard::display_reg()
{
    ui->line_reg0->setText(QString::number(this->adf4351->reg[0], 16).toUpper());
    ui->line_reg1->setText(QString::number(this->adf4351->reg[1], 16).toUpper());
    ui->line_reg2->setText(QString::number(this->adf4351->reg[2], 16).toUpper());
    ui->line_reg3->setText(QString::number(this->adf4351->reg[3], 16).toUpper());
    ui->line_reg4->setText(QString::number(this->adf4351->reg[4], 16).toUpper());
    ui->line_reg5->setText(QString::number(this->adf4351->reg[5], 16).toUpper());

    if (this->enable_auto_tx)
        emit signal_auto_tx();
}


/*******************************************************************************
 *  USB device management and GUI update
 ******************************************************************************/

/**
 * @brief Called by HID_PnP whenever the USB connection state or device
 *        data changes.  Updates the window title, RF button, device list,
 *        and pushes pending commands (flash write, identify, macro program).
 */
void USBIOBoard::update_gui(bool isConnected, UI_Data *ui_data)
{
    ui->label_device_busy->setVisible(false);
    this->isDeviceConnected = isConnected;

    if (isConnected)
    {
        /* Update window title with firmware version */
        if (!ui_data->isReadFirmwareInfoPending)
        {
            this->setWindowTitle(
                QString("RFGEN44 ") + APP_VERSION + " RF GEN: FW : "
                + QString::number(ui_data->firmware_version_major) + "."
                + QString::number(ui_data->firmware_version_minor) + ":"
                + QString::number(ui_data->firmware_build_number)
                + " Serial: " + ui_data->selected_usb_device + " Connected"
            );

            /* Update firmware/serial info below pictogram */
            ui->label_firmwareInfo->setText(
                QString("FW: %1.%2.%3  |  SN: %4")
                    .arg(ui_data->firmware_version_major)
                    .arg(ui_data->firmware_version_minor)
                    .arg(ui_data->firmware_build_number)
                    .arg(ui_data->selected_usb_device)
            );
        }

        /* Update RF button and busy indicator */
        if (!ui_data->isReadFirmwareInfoPending)
        {
            ui->RF_CTRL->setText(ui_data->RF_OUT ? "RF : ON" : "RF : OFF");
            ui->label_device_busy->setVisible(ui_data->device_busy);
            updateDeviceDiagram(ui_data->RF_OUT);
            this->lastKnownRFOn = ui_data->RF_OUT;
            this->deviceFirmwareMajor = ui_data->firmware_version_major;

            /* Program Flash (Device Settings): works on all firmware,
             * only disabled during sweep/macro */
            if (sweep_timer->isActive())
            {
                ui->pushButton_program_flash->setEnabled(false);
                ui->pushButton_program_flash->setToolTip("Not available while sweep is active");
            }
            else if (isMacroRunning)
            {
                ui->pushButton_program_flash->setEnabled(false);
                ui->pushButton_program_flash->setToolTip("Not available while macro is running");
            }
            else
            {
                ui->pushButton_program_flash->setEnabled(true);
                ui->pushButton_program_flash->setToolTip("Save current settings to device flash");
            }

            /* Macro Program to Flash: requires firmware > 2 */
            if (ui_data->firmware_version_major < 2)
            {
                pushButton_macroProgram->setEnabled(false);
                pushButton_macroProgram->setToolTip("Only supported with Device Firmware 2.0+\nPlease update firmware.");
            }
            else if (sweep_timer->isActive())
            {
                pushButton_macroProgram->setEnabled(false);
                pushButton_macroProgram->setToolTip("Not available while sweep is active");
            }
            else if (isMacroRunning)
            {
                /* already disabled by macro_start_click */
            }
            else
            {
                pushButton_macroProgram->setEnabled(true);
                pushButton_macroProgram->setToolTip("Write macro sequence to device flash (requires firmware 2.0+)");
            }

            /* Macro Start: disabled during sweep/macro */
            if (sweep_timer->isActive())
            {
                pushButton_macroStart->setEnabled(false);
                pushButton_macroStart->setToolTip("Not available while sweep is active");
            }
            else if (isMacroRunning)
            {
                /* already disabled by macro_start_click */
            }
            else
            {
                pushButton_macroStart->setEnabled(true);
                pushButton_macroStart->setToolTip("Start executing macro sequence from step 1");
            }

            /* Enable basic controls when connected (unless sweep/macro overrides) */
            if (!sweep_timer->isActive() && !isMacroRunning)
            {
                ui->USBTX->setEnabled(!ui->checkBox_autotx->isChecked());
                ui->USBTX->setToolTip("Manually send current register values to device via USB");
                ui->RF_CTRL->setEnabled(true);
                ui->RF_CTRL->setToolTip("Toggle RF output on / off");
                ui->pushButton_sweep_start->setEnabled(true);
                ui->pushButton_sweep_start->setToolTip("Start frequency sweep");
                ui->pushButton_sweep_stop->setEnabled(true);
                ui->pushButton_sweep_stop->setToolTip("Stop frequency sweep");
                ui->pushButton_erase_flash->setEnabled(true);
                ui->pushButton_erase_flash->setToolTip("Erase all saved settings from device flash");
                ui->pushButton_identify->setEnabled(true);
                ui->pushButton_identify->setToolTip("Blink LED on the selected device for identification");
            }
        }
    }
    else
    {
        /* Device disconnected */
        ui_data->readRFCTRL_pending        = true;
        ui_data->isReadFirmwareInfoPending = true;

        this->setWindowTitle(
            QString("RFGEN44 ") + APP_VERSION + " RF GEN : Device Not Found"
        );
        ui->RF_CTRL->setText("RF : XX");
        updateDeviceDiagram(false);
        ui->label_firmwareInfo->setText("FW: --  |  SN: --");

        if (sweep_timer->isActive())
            sweep_stop_click();

        ui->pushButton_program_flash->setEnabled(false);
        ui->pushButton_program_flash->setToolTip("Device not connected");
        ui->pushButton_erase_flash->setEnabled(false);
        ui->pushButton_erase_flash->setToolTip("Device not connected");
        ui->USBTX->setEnabled(false);
        ui->USBTX->setToolTip("Device not connected");
        ui->RF_CTRL->setEnabled(false);
        ui->RF_CTRL->setToolTip("Device not connected");
        ui->pushButton_sweep_start->setEnabled(false);
        ui->pushButton_sweep_start->setToolTip("Device not connected");
        ui->pushButton_sweep_stop->setEnabled(false);
        ui->pushButton_sweep_stop->setToolTip("Device not connected");
        pushButton_macroStart->setEnabled(false);
        pushButton_macroStart->setToolTip("Device not connected");
        pushButton_macroProgram->setEnabled(false);
        pushButton_macroProgram->setToolTip("Device not connected");
        ui->pushButton_identify->setEnabled(false);
        ui->pushButton_identify->setToolTip("Device not connected");
        this->deviceFirmwareMajor = 0;

        if (isMacroRunning)
            macro_stop_click();
    }

    /* ---- USB device list management ---- */
    this->usb_device_list = ui_data->usb_device_list;

    if (ui_data->isDeviceListChanged)
    {
        this->usb_device_list_poplated = false;

        ui->comboBox_usb_devices->blockSignals(true);
        ui->comboBox_usb_devices->clear();
        this->usb_device_list.removeDuplicates();
        ui->comboBox_usb_devices->addItems(this->usb_device_list);

        if (this->selected_usb_device.length() > 0)
        {
            int idx = ui->comboBox_usb_devices->findText(this->selected_usb_device);
            if (idx >= 0)
            {
                ui->comboBox_usb_devices->setCurrentIndex(idx);
            }
            else
            {
                this->selected_usb_device  = this->usb_device_list.first();
                this->isSelctedDeviceChange = true;
            }
        }
        else
        {
            this->selected_usb_device  = this->usb_device_list.first();
            this->isSelctedDeviceChange = true;
        }

        ui->comboBox_usb_devices->blockSignals(false);
        this->usb_device_list_poplated = true;
    }

    /* ---- Push state back to HID_PnP ---- */
    ui_data->selected_usb_device  = this->selected_usb_device;
    ui_data->isSelctedDeviceChange = this->isSelctedDeviceChange;

    if (ui_data->isDeviceChangeDone)
    {
        this->isSelctedDeviceChange   = false;
        ui_data->isDeviceChangeDone   = false;
        ui_data->readRFCTRL_pending   = true;
        this->lastKnownRFOn           = false;
        emit singal_recalculate();
    }

    /* Flash program pending */
    if (this->isFlashProgramPending)
    {
        this->isFlashProgramPending     = false;
        ui_data->isFlashWriteRequested  = true;
        ui_data->isDeviceCtrlPending    = true;
    }

    ui_data->autoStartatBoot = this->isAutoStartEnabled;

    /* Identify LED blink pending */
    if (this->isIdentifyCalled)
    {
        this->isIdentifyCalled          = false;
        ui_data->isIdentfiyLEDRequested = true;
        ui_data->isDeviceCtrlPending    = true;
    }

    /* ---- Copy ADF4351 parameters to HID struct (fixed-point conversion) ---- */
    ui_data->adf4351.frequency      = static_cast<uint32_t>(this->adf4351->frequency  * 100);
    ui_data->adf4351.start_freq     = static_cast<uint32_t>(this->adf4351->start_freq * 100);
    ui_data->adf4351.stop_freq      = static_cast<uint32_t>(this->adf4351->stop_freq  * 100);
    ui_data->adf4351.step_freq      = static_cast<uint32_t>(this->adf4351->step_freq  * 100);
    ui_data->adf4351.step_ms        = static_cast<uint16_t>(this->adf4351->step_ms);
    ui_data->adf4351.aux_select     = static_cast<uint16_t>(this->adf4351->aux_select);
    ui_data->adf4351.isSweepEnabled = static_cast<uint8_t>(this->adf4351->isSweepEnabled);
    ui_data->adf4351.isStartOnBoot  = static_cast<uint8_t>(this->adf4351->isStartOnBoot);
    ui_data->adf4351.isStartOfSweep = static_cast<uint8_t>(this->adf4351->isStartOfSweep);
    ui_data->adf4351.ref_freq       = static_cast<uint32_t>(this->adf4351->ref_freq   * 100);

    /* Erase flash pending */
    if (this->isEraseFlashRequested)
    {
        this->isEraseFlashRequested       = false;
        ui_data->isEraseFlashRequested    = true;
    }

    /* Serial number write pending */
    if (this->isWriteSerialNumberRequested)
    {
        this->isWriteSerialNumberRequested      = false;
        ui_data->isWriteSerialNumberRequested   = true;
    }

    ui_data->deviceinfo.serialNumber = this->serialNumber;

    /* ---- Macro flash program pending ---- */
    if (this->isMacroProgramPending)
    {
        this->isMacroProgramPending = false;
        memcpy(&ui_data->deviceMacro[0], &this->macroBlock0, sizeof(devicemacro_s));
        memcpy(&ui_data->deviceMacro[1], &this->macroBlock1, sizeof(devicemacro_s));
        emit signal_write_macro();
    }
}


/*******************************************************************************
 *  Misc UI slots
 ******************************************************************************/

void USBIOBoard::comboBox_device_selection_changed()
{
    if (this->usb_device_list_poplated && ui->comboBox_usb_devices->count() != 0)
    {
        this->selected_usb_device  = ui->comboBox_usb_devices->currentText();
        this->isSelctedDeviceChange = true;
    }
}


void USBIOBoard::autotx_clicked()
{
    this->enable_auto_tx = ui->checkBox_autotx->isChecked();
    ui->USBTX->setEnabled(!this->enable_auto_tx);
}


void USBIOBoard::autoStartonBoot_clicked()
{
    this->isAutoStartEnabled = ui->checkBox_autoLockOnBoot->isChecked();
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
    this->isAutoStartEnabled = ui->checkBox_autoLockOnBoot->isChecked();
    emit singal_recalculate();
    update_reg();
}
