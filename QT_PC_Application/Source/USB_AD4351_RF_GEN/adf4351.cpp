#include "adf4351.h"
#include <math.h>
#include <stdlib.h>


ADF4351::ADF4351() {
this->enable_gcd = true;
}


inline int min(const int a, const int b)
{
    if (a > b)
        return b;
    return a;
}

uint32_t gcd(uint32_t a, uint32_t b)
{
    while ( true)
    {
        if (a == 0)
        {
            return b;
        }
        else if (b == 0)
        {
            return a;
        }
        else if ( a > b)
        {
            a = a % b;
        }
        else
        {
            b = b % a;
        }
    }
}

void ADF4351::ADF4351_calculte_reg_from_freq(uint32_t frequency)
{
    this->frequency = frequency;
    double pfd_freq = (this->ref_freq *(this->ref_doubler?2:1))/((this->ref_div2?2:1) * this->r_counter);
    uint32_t output_divider;
    double N;
    for (uint8_t i; i < 7 ; i++)
    {
        output_divider = 2 ^ i;
        if ( (2200.0 / output_divider) <= frequency)
        {
            break;
        }
    }

    if (this->feedback_select == false)
    {
        N = frequency * output_divider / pfd_freq;
    }
    else
    {
        N = frequency / pfd_freq;
    }

    uint32_t INT = floor(N);
    uint32_t MOD = round(1000.0 * pfd_freq);
    uint32_t FRAC = round((N - INT) * MOD);

    if (this->enable_gcd)
    {
        uint32_t div= gcd(MOD, FRAC);
        MOD = MOD / div;
        FRAC = FRAC / div;
    }

    if (MOD == 1)
    {
        MOD = 2;
    }

    if (pfd_freq > 32.0)
    {
        if (FRAC !=0)
        {
        //   printf("Maximum PFD frequency in Frac-N mode (FRAC != 0) is 32MHz.\n");

        }
        else if (this->band_select_clock_mode == true)
        {
          //  printf("Band Select Clock Mode must be set to High when PFD is >32MHz in Int-N mode (FRAC = 0).\n");
        }
    }

    if (!this->band_select_clock_divider)
    {
        uint32_t pfd_scale  = 8 ;

        if (this->band_select_clock_mode != 0)
        {
                pfd_scale = 2;
        }
        if( this->band_select_clock_mode == 0)
        this->band_select_clock_divider = min(ceil(8*pfd_freq), 255);
    }

    this->band_select_clock_freq = (1000.0 * pfd_freq) / this->band_select_clock_divider;

    if (this->band_select_clock_freq > 500.0)
    {
       // printf("Band Select Clock Frequency is too High. It must be 500kHz or less.\n");

    }
    else if (this->band_select_clock_freq > 125.0)
    {
        if (this->band_select_clock_mode == 0)
        {
           // printf("Band Select Clock Frequency is too high. Reduce to 125kHz or less, or set Band Select Clock  Mode to High. \n");

        }
    }

    this->reg[0] = INT<<15 | ((FRAC << 3) & 0x7FF8) | 0x1;
    this->reg[1] = this->PHASE_ADJUST | this->PR1 << 27 | (this->PHASE<<5 & 0x7FF800) | ((MOD << 3) & 0x1F8) | 0x2;
}


void ADF4351::BuildRegisters()
{
    for (int i = 0; i < 6; i++)
    {
        this->previous_reg[i] = this->reg[i];
    }
    this->PFDFreq = (this->ref_freq * (double)(this->ref_doubler ? 2 : 1) / (double)(this->ref_div2 ? 2 : 1) / (double)this->r_counter);
    uint16_t output_divider = 1;
    if (this->frequency >= 2200.0)
    {
        output_divider = 1;
    }
    if (this->frequency < 2200.0)
    {
        output_divider = 2;

    }
    if (this->frequency < 1100.0)
    {
        output_divider = 4;
    }
    if (this->frequency < 550.0)
    {
        output_divider = 8;
    }
    if (this->frequency < 275.0)
    {
        output_divider = 16;
    }
    if (this->frequency < 137.5)
    {
        output_divider = 32;
    }
    if (this->frequency < 68.75)
    {
        output_divider = 64;
    }


    if (this->feedback_select )
    {
        this->N = (double)this->frequency * output_divider / this->PFDFreq;
    }
    else
    {
        this->N = (double)this->frequency / this->PFDFreq;
    }

    this->INT = (uint32_t)this->N;
    this->MOD = (uint32_t) round(1000 * this->PFDFreq);
    this->FRAC = (uint32_t)round(((double)this->N - this->INT) * this->MOD);
    if (this->enable_gcd)
    {
        uint32_t div = gcd((uint32_t)this->MOD, (uint32_t)this->FRAC);
        this->MOD = this->MOD / div;
        this->FRAC = this->FRAC / div;
    }
    if (this->MOD == 1.0)
    {
        this->MOD = 2.0;
    }


   // this.RFoutBox.Text = ((this.INT + this.FRAC / this.MOD) * (double)this.PFDFreq / (double)Convert.ToInt16(this.DivBox.Text) * (double)((this.FeedbackSelectBox.SelectedIndex == 1) ? 1 : Convert.ToInt16(this.DivBox.Text))).ToString();

    if (this->band_select_auto)
    {
        uint32_t temp ;
        if (this->band_select_clock_mode == 0) //low
        {
            temp = (uint32_t)round(8.0 * this->PFDFreq);
            if (8.0 * this->PFDFreq - temp > 0)
            {
                temp += 1u;
            }
            temp = ((temp > 255u) ? 255u : temp);
        }
        else
        {
            temp = (uint32_t)round(this->PFDFreq * 2.0);
            if (2.0 * this->PFDFreq - temp > 0)
            {
                temp += 1u;
            }
            temp = ((temp > 255u) ? 255u : temp);
        }
        this->band_select_clock_divider = temp;
        this->band_select_clock_freq = (1000 * this->PFDFreq / (uint32_t)temp);
    }



    this->reg[0] = (uint32_t)((double)((uint32_t)this->INT & 65535) * (1<<15) + (double)((uint32_t)this->FRAC & 4095) * (1<<3) + 0.0);
    this->reg[1] = (uint32_t)((double)this->PHASE_ADJUST* (1 << 28) + (double)this->PR1 * (1 << 27) + (double)this->PHASE * (1 << 15) + (double)((int)this->MOD & 4095) * (1 << 3) + 1.0);


    this->reg[2] = (uint32_t)((double)this->low_noise_spur_mode * (1 << 29) + (double)this->muxout * (1 << 26) + (double)(this->ref_doubler ? 1 : 0) * (1 << 25) + (double)(this->ref_div2 ? 1 : 0) * (1 << 24) + (double)this->r_counter * (1 << 14) + (double)this->double_buff * (1 << 13) + (double)this->charge_pump_current * (1 << 9) + (double)this->LDF * (1 << 8) + (double)this->LDP  * (1 << 7) + (double)this->PD_Polarity * (1 << 6) + (double)this->power_down * (1 << 5) + (double)this->cp_3stage * (1 << 4) + (double)this->counter_reset * (1 << 3) + 2.0);
    this->reg[3] = (uint32_t)((uint32_t)this->band_select_clock_mode * (1 << 23) + (uint32_t)this->ABP * (1 << 22) + (uint32_t)this->charge_cancelletion * (1 << 21) + (uint32_t)this->CSR * (1 << 18) + (uint32_t)this->CLK_DIV_MODE * (1 << 15) + (uint32_t)this->clock_divider * (1 << 3) + 3.0);

    this->reg[4] = (uint32_t)((uint32_t)this->feedback_select * (1 << 23)  + log2((double)output_divider) * (1 << 20) + (double)this->band_select_clock_divider * (1 << 12) + (double)this->VCO_power_down * (1 << 11) + (double)this->MTLD * (1 << 10) + (double)this->AUX_output_mode * (1 << 9) + (double)this->AUX_output_enable * (1 << 8) + (double)this->AUX_output_power * (1 << 6) + (double)this->RF_OUT * (1 << 5) + (double)this->RF_output_power * (1 << 3) + 4.0);
    this->reg[5] = (uint32_t)((uint32_t)this->LD * (1 << 22) + (uint32_t)0x3 * (1 << 19) + 5.0);


//    this.UpdateVCOChannelSpacing();
//    this.UpdateVCOOutputFrequencyBox();
//    if (this.CLKDivModeBox.SelectedIndex == 2)
//    {
//        this.TsyncLabel.Visible = true;
//        this.TsyncLabel.Text = "Tsync = " + (1.0 / (double)this.PFDFreq * this.MOD * (double)Convert.ToInt32(this.ClockDividerValueBox.Value)).ToString() + " us";
//    }
//    else
//    {
//        this.TsyncLabel.Visible = false;
//    }
//    if (this.Autowrite.Checked)
//    {
//        this.WriteAllButton.PerformClick();
//    }

//    this.Limit_Check();
//    if (this.FeedbackSelectBox.SelectedIndex == 0)
//    {
//        this.FeedbackFrequencyLabel.Text = Convert.ToDouble(this.RFoutBox.Text) + " MHz";
//    }
//    else
//    {
//        this.FeedbackFrequencyLabel.Text = (Convert.ToDouble(this.RFoutBox.Text) * (double)Convert.ToInt16(this.OutputDividerBox.Text)).ToString() + " MHz";
//    }

 //   this.WarningsCheck();
    emit reg_update_result();
}
