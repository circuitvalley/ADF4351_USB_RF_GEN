#ifndef ADF4351_H
#define ADF4351_H

#include <stdint.h>
#include <qobject.h>
#include <QObject>
#include <QTimer>
#include "../HIDAPI/hidapi.h"

#include <wchar.h>
#include <string.h>
#include <stdlib.h>

typedef enum aux_ctrl_e
{
    AUX_SYNC_OUT,
    AUX_SYNC_IN,
    AUX_EXT_REF_IN
}aux_ctrl_t;

class ADF4351 : public QObject
{
    Q_OBJECT
private:

    uint32_t band_select_clock_divider;

public:
    ADF4351();

    double      ref_freq;
    bool        ref_doubler;
    bool        ref_div2;
    bool        enable_gcd;
    bool        feedback_select; //0: divided or 1:fundamental  , 1 default
    bool        band_select_clock_mode;
    uint32_t    clock_divider;
    double      band_select_clock_freq;
    bool        band_select_auto;
    double      N;
    double      PFDFreq;
    bool        PHASE_ADJUST;  //0 default
    bool        PR1;   //1 default 8/9
    uint8_t     low_noise_spur_mode;  //0 default
    uint8_t     muxout; //0 default
    uint8_t     charge_pump_current;  //8 default
    bool        LDF;   //0: fract n 1: INT n , 0 default
    bool        LDP;   //0: 10ns 1: 6ns 0 default
    bool        PD_Polarity; //0: negative 1:positive 1 default
    bool        power_down; //0 default
    bool        cp_3stage; //0 default
    bool        counter_reset; //0 default
    bool        double_buff; //0 default
    bool        ABP; //0 6ns fractn, 1: 3ns intn ,0 default
    bool        charge_cancelletion; //0: dislabe default
    bool        CSR; //0: dislabe default
    uint8_t     CLK_DIV_MODE; //0 default clock div off, 1 fast lock, 2  Resync enable , 3 reserve
    uint8_t     LD; //0  low , 1 default lock detect 2 low , 3 high
    bool        VCO_power_down; //0 disable default
    bool        MTLD; //0 disable default
    bool        AUX_output_mode; //0 divided default 1: fundamental
    bool        AUX_output_enable; //0 disable 1:enable
    uint8_t     AUX_output_power;
    uint8_t     RF_output_power; //3 default +5dBm 0: -4dBm
    bool        RF_OUT; //1:enable default,
    uint32_t    PHASE;
    uint32_t    r_counter;
    double      frequency;
    double      start_freq;
    double      stop_freq;
    double      step_freq;
    uint16_t    aux_select;
    bool        isSweepEnabled;
    bool        isHopEnabled;
    bool        isStartOnBoot;
    bool        isStartOfSweep;
    bool        isLockDetected;

    uint32_t    step_ms;
    uint32_t    reg[6];
    uint32_t    previous_reg[6];
    uint32_t    INT;
    double      MOD;
    double      FRAC;
    void        ADF4351_calculte_reg_from_freq(uint32_t frequency);

public slots:
    void BuildRegisters();
signals:
    void reg_update_result();
};

#endif // ADF4351_H
