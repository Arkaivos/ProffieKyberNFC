#ifdef CONFIG_TOP

#include "proffieboard_config.h"

// Configuración básica.
#define NUM_BLADES 2  // Dos blades, el filo y el cristal.
#define NUM_BUTTONS 1
#define VOLUME 1500
const unsigned int maxLedsPerStrip = 115; //109+6

// Hardware.
#define CLASH_THRESHOLD_G 1.0
#define ENABLE_AUDIO
#define ENABLE_MOTION
#define ENABLE_WS2811
#define ENABLE_SD
#define SAVE_STATE
#define ENABLE_ALL_EDIT_OPTIONS

// Desactivamos cambio de color por botón.
#define DISABLE_COLOR_CHANGE

// Habilitamos I2C para NFC.
#define ENABLE_I2C
// Tiempo en segundos que va a estar activa la lectura del NFC tras iniciar la placa o apagar el filo (0 = ilimitado)
#define NFC_TIMEOUT 60

// Activamos el cristal cuando se active el filo.
//#define CRYSTAL_EDGE_ACTIVATION

#endif

#ifdef CONFIG_PROP
#include "../props/kyber_nfc.h"
#undef PROP_TYPE
#define PROP_TYPE KyberNFC  // Seleccionamos este prop.
#endif

#ifdef CONFIG_PRESETS

// Destinamos más memoria al efecto complejo del preset.
#define EXTRA_COLOR_BUFFER_SPACE 60


Preset presets[] = {
  { "Default", "tracks/default.wav",
    // Estilo del filo.
    StylePtr<InOutHelper<RgbArg<1,Rgb<120,120,120>>, 300, 500>>(),
    // Estilo del cristal.
    StylePtr<Black>(),
    "default"
  },
  { "Subdued", "tracks/default.wav",
    // Fett263 Rotoscope Style - Adaptado para NFC
    //   Solo el color base (argumento 1) se modifica por NFC
    //   Resto de efectos usan colores por defecto

    StylePtr<Layers<
      Mix<PulsingF<Sin<Int<30>,Int<4000>,Int<10000>>>,
        RgbArg<1,Rgb<120,120,120>>,  // COLOR BASE BLANCO por defecto
        Stripes<12000,-400,
          RgbArg<1,Rgb<120,120,120>>,
          RgbArg<1,Rgb<120,120,120>>,
          Mix<Int<7710>,Black,RgbArg<1,Rgb<120,120,120>>>,
          RgbArg<1,Rgb<120,120,120>>,
          Mix<Int<16448>,Black,RgbArg<1,Rgb<120,120,120>>>>
      >,
      TransitionEffectL<TrWaveX<Rgb<255,255,255>,
        Scale<EffectRandomF<EFFECT_BLAST>,Int<100>,Int<400>>,Int<100>,
        Scale<EffectPosition<EFFECT_BLAST>,Int<100>,Int<400>>,
        Scale<EffectPosition<EFFECT_BLAST>,Int<28000>,Int<8000>>>,EFFECT_BLAST>,
      Mix<IsLessThan<ClashImpactF<>,Int<26000>>,
        TransitionEffectL<TrConcat<TrInstant,
          AlphaL<Rgb<255,255,255>,
            Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,
              Scale<ClashImpactF<>,Int<12000>,Int<60000>>>>,
          TrFadeX<Scale<ClashImpactF<>,Int<200>,Int<400>>>>,EFFECT_CLASH>,
        TransitionEffectL<TrWaveX<Rgb<255,255,255>,
          Scale<ClashImpactF<>,Int<100>,Int<400>>,Int<100>,
          Scale<ClashImpactF<>,Int<100>,Int<400>>,
          Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>>,EFFECT_CLASH>
      >,
      LockupTrL<AlphaL<AlphaMixL<Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,
          Scale<SwingSpeed<100>,Int<14000>,Int<22000>>>,
        AudioFlicker<Rgb<255,255,255>,Mix<Int<12000>,Black,Rgb<255,255,255>>>,
        BrownNoiseFlicker<Rgb<255,255,255>,Mix<Int<12000>,Black,Rgb<255,255,255>>,300>>,
        Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,
          Scale<SwingSpeed<100>,Int<14000>,Int<22000>>>>,
        TrConcat<TrJoin<TrDelay<50>,TrInstant>,
          Mix<IsLessThan<ClashImpactF<>,Int<26000>>,Rgb<255,255,255>,
            AlphaL<Rgb<255,255,255>,
              Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,
                Scale<ClashImpactF<>,Int<20000>,Int<60000>>>>>,
          TrFade<300>>,
        TrConcat<TrInstant,Rgb<255,255,255>,TrFade<400>>,
        SaberBase::LOCKUP_NORMAL>,
      ResponsiveLightningBlockL<Strobe<Rgb<255,255,255>,AudioFlicker<Rgb<255,255,255>,Blue>,50,1>,
        TrConcat<TrInstant,
          AlphaL<Rgb<255,255,255>,Bump<Scale<BladeAngle<>,Int<10000>,Int<21000>>,Int<10000>>>,
          TrFade<200>>,
        TrConcat<TrInstant,Rgb<255,255,255>,TrFade<400>>>,
      LockupTrL<AlphaL<TransitionEffect<RandomPerLEDFlickerL<Rgb<255,255,255>>,
        BrownNoiseFlickerL<Rgb<255,255,255>,Int<300>>,
        TrInstant,TrFade<4000>,EFFECT_DRAG_BEGIN>,
        SmoothStep<Int<28000>,Int<3000>>>,
        TrWipeIn<200>,TrWipe<200>,SaberBase::LOCKUP_DRAG>,
      LockupTrL<AlphaL<Stripes<2000,4000,
        Mix<TwistAngle<>,Rgb<255,255,255>,RotateColorsX<Int<3000>,Rgb<255,255,255>>>,
        Mix<Sin<Int<50>>,Black,Mix<TwistAngle<>,Rgb<255,255,255>,RotateColorsX<Int<3000>,Rgb<255,255,255>>>>,
        Mix<Int<4096>,Black,Mix<TwistAngle<>,Rgb<255,255,255>,RotateColorsX<Int<3000>,Rgb<255,255,255>>>>>,
        SmoothStep<Int<28000>,Int<3000>>>,
        TrConcat<TrWipeIn<600>,
          AlphaL<HumpFlicker<Mix<TwistAngle<>,Rgb<255,255,255>,RotateColorsX<Int<3000>,Rgb<255,255,255>>>,
            RotateColorsX<Int<3000>,Mix<TwistAngle<>,Rgb<255,255,255>,RotateColorsX<Int<3000>,Rgb<255,255,255>>>>,100>,
            SmoothStep<Int<28000>,Int<3000>>>,
          TrSmoothFade<600>>,
        TrWipe<200>,SaberBase::LOCKUP_MELT>,
      InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>
    >>(),
    // Estilo del cristal.
    StylePtr<Black>(),
    "subdued"
  }
};

// Configuración de los dos blades.
BladeConfig blades[] = {
  { 0,
    WS281XBladePtr<115, bladePin, Color8::GRB, PowerPINS<bladePowerPin2, bladePowerPin3>>(),
    WS281XBladePtr<1, blade4Pin, Color8::GRB, PowerPINS<bladePowerPin4>>(),
    CONFIGARRAY(presets)
  }
};

#endif

#ifdef CONFIG_BUTTONS
Button PowerButton(BUTTON_POWER, powerButtonPin, "pow");
#endif