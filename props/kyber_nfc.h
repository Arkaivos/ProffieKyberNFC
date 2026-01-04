#ifndef PROPS_KYBER_NFC_H
#define PROPS_KYBER_NFC_H

#include "prop_base.h"
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "../common/i2cbus.h"

extern I2CBus i2cbus;

// =========================================
// KYBER NFC PROP - Sistema de cristales NFC
// =========================================
class KyberNFC : public PropBase {
private:
  static constexpr const char* DEFAULT_PRESET_NAME = "default";
  Adafruit_PN532* nfc;
  bool nfcInitialized;
  bool nfcActive;
  
  uint8_t lastUID[7];
  uint8_t lastUIDLength;
  uint8_t nfcColor[3];
  bool tagCurrentlyPresent;
  char nfcPresetName[32];
  uint32_t lastCheckTime;
  uint32_t nfcActiveStartTime; 
  bool crystalLEDOn = false;       // Indica si el cristal está encendido.
  uint32_t crystalLEDOnTime = 0;   // Momento en que se encendió el cristal.
  bool needsPresetReload = true;   // La primera vez que arranca necesita cargar el preset de la SD.
  
  // IDs de los diferentes Blades que componen el sistema.
  static constexpr int MAIN_BLADE = 1;
  static constexpr int CRYSTAL_CHAMBER = 2;

  // Estilo personalizado para el LED del cristal.
  class CrystalLEDStyle : public BladeStyle {
  public:
    CrystalLEDStyle(uint16_t r, uint16_t g, uint16_t b, uint32_t durationMs)
      : r_(r), g_(g), b_(b), duration_(durationMs), startTime_(millis()) {}

    bool isFinished() const {
      if (duration_ == 0) return false; // Permanente nunca termina.
      return (millis() - startTime_) >= duration_; // Termina al finalizar la duración.
    }

    bool isTemporaryActive() const {
        // Indica si ha terminado la animación de feedback del cristal cuando esta no es ilimitada.
        return duration_ > 0 && (millis() - startTime_) < duration_;
    }

    void run(BladeBase* blade) override {
      uint32_t elapsed = millis() - startTime_;

      // Encendido permanente (duration = 0).
      if (duration_ == 0) {
        runPulsing(blade);
        return;
      }

      // Encendido temporal.
      if (elapsed < duration_ - 1000) {
        runPulsing(blade);
        return;
      }

      // Fade out.
      if (elapsed < duration_) {
        uint32_t fadeProgress = elapsed - (duration_ - 1000);
        uint16_t brightness = 65535 - ((fadeProgress * 65535) / 1000);

        uint16_t r = (r_ * brightness) >> 16;
        uint16_t g = (g_ * brightness) >> 16;
        uint16_t b = (b_ * brightness) >> 16;

        blade->set(0, Color16(r, g, b));
        return;
      }

      // Apagado - todos los LEDs del cristal en negro.
      for (int i = 0; i < blade->num_leds(); i++) {
        blade->set(i, Color16(0, 0, 0));
      }
    }

    // IMPORTANTE: Solo mantener con energía mientras está activo.
    // Si se deja con energía siempre, produce sobrecalentamiento y consume más batería.
    bool NoOnOff() override { 
      if (duration_ == 0) {
        return true; // Permanente, con energía mientras dure el estilo.
      }
      
      uint32_t elapsed = millis() - startTime_;
      return elapsed < duration_; // Al finalizar la duración, permite desconectar el cristal de la batería.
    }
    
    bool IsHandled(HandledFeature feature) override { return false; }

  private:
    void runPulsing(BladeBase* blade) {
      uint32_t phase = (millis() * 65536) / 1500;
      uint16_t brightness = (sin_table[(phase >> 8) & 0xFF] + 32768);

      uint16_t r = (r_ * brightness) >> 16;
      uint16_t g = (g_ * brightness) >> 16;
      uint16_t b = (b_ * brightness) >> 16;

      blade->set(0, Color16(r, g, b));
    }

    uint16_t r_, g_, b_;
    uint32_t duration_;
    uint32_t startTime_;
  };
  
  CrystalLEDStyle* crystalStyle_;
  BladeStyle* savedCrystalStyle_;
  
public:
  KyberNFC() : PropBase(), lastUIDLength(0), lastCheckTime(0), 
               nfcInitialized(false), nfcActive(false), tagCurrentlyPresent(false),
               nfcActiveStartTime(0), crystalStyle_(nullptr), savedCrystalStyle_(nullptr) {
    nfc = new Adafruit_PN532(255, 255);
    nfcColor[0] = 255;
    nfcColor[1] = 255;
    nfcColor[2] = 255;
    strcpy(nfcPresetName, DEFAULT_PRESET_NAME);
  }
  
  ~KyberNFC() {
    if(nfc) delete nfc;
    if(crystalStyle_) delete crystalStyle_;
  }
  
  const char* name() override { return "KyberNFC"; }
  
  void Loop() override {
    PropBase::Loop();
    
    if(!nfcInitialized) {
      initNFC();
      if(nfcInitialized) {
        activateNFC();
      }
    }

    // Limpiar estilo del cristal cuando termine
    if (crystalLEDOn && crystalStyle_ && crystalStyle_->isFinished()) {
      deactivateCrystalLED();
    }

    // Verificar timeout del NFC
    if (nfcActive && NFC_TIMEOUT > 0) {
      uint32_t elapsed = (millis() - nfcActiveStartTime) / 1000;  // Convertir a segundos
      if (elapsed >= NFC_TIMEOUT) {
        STDOUT.println(String("-- NFC timeout reached (") + NFC_TIMEOUT + "s), putting NFC to sleep");
        deactivateNFC();
      }
    }

    if(!SaberBase::IsOn() && nfcInitialized && nfcActive) {
      processNFC();
    }
  }
  
  bool Event2(enum BUTTON button, EVENT event, uint32_t modifiers) override {
    switch (EVENTID(button, event, modifiers)) {
      case EVENTID(BUTTON_POWER, EVENT_CLICK_SHORT, MODE_OFF):
        On();

        if (needsPresetReload) {
          needsPresetReload = false;
          
          #ifdef ENABLE_SD
          if (LSFS::Exists("cur_kyber.txt")) {
            File f = LSFS::Open("cur_kyber.txt");
            if (f) {
              uint8_t buffer[16];
              size_t bytesRead = f.read(buffer, sizeof(buffer) - 1);
              buffer[bytesRead] = '\0';
              f.close();
              
              int savedPreset = atoi((char*)buffer);
              STDOUT.println(String("-- Loading saved preset: ") + savedPreset);
              SetPreset(savedPreset, true);
              STDOUT.println("-- Preset reloaded");
            }
          }
          #endif
        }

        #ifdef CRYSTAL_EDGE_ACTIVATION
        activateCrystalLED(0);
        #endif

        return true;
        
      case EVENTID(BUTTON_POWER, EVENT_HELD_MEDIUM, MODE_ON):
        // Que no se apague si el cristal está mostrando un feedback de activación temporal.
        if (crystalStyle_ && crystalStyle_->isTemporaryActive()) {
            STDOUT.println("-- Saber off ignored (crystal feedback still active)");
            return true;  // Ignoramos el OFF
        }

        Off();

        // Reactivar el timeout del NFC al apagar el filo
        if (nfcInitialized && !nfcActive) {
          activateNFC();
        } else if (nfcInitialized && nfcActive) {
          // Resetear el timeout si ya está activo
          nfcActiveStartTime = millis();
          STDOUT.println("-- NFC timeout reset");
        }

        #ifdef CRYSTAL_EDGE_ACTIVATION
        deactivateCrystalLED();
        #endif

        return true;
        
      default:
        return false;
    }
  }
  
private:
  void initNFC() {
    STDOUT.println("Initializing NFC...");
    nfc->begin();
    
    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
      STDOUT.println("-- NFC Module not found");
      nfcInitialized = false;
      return;
    }
    
    STDOUT.print("Found PN532 modul.");
    STDOUT.println((versiondata >> 24) & 0xFF, HEX);
    
    nfc->SAMConfig();
    nfcInitialized = true;
  }

  // Activar el NFC
  void activateNFC() {
    if (!nfcActive) {
      STDOUT.println("-- Activating NFC");
      nfc->SAMConfig();  // Despertar el módulo NFC
      i2cbus.inited();   // Asegurar que I2C está activo
      nfcActive = true;
      nfcActiveStartTime = millis();
      
      if (NFC_TIMEOUT > 0) {
        STDOUT.println(String("-- NFC will sleep after ") + NFC_TIMEOUT + " seconds");
      } else {
        STDOUT.println("-- NFC timeout disabled (unlimited)");
      }
    }
  }

  // Desactivar el NFC (modo sleep)
  void deactivateNFC() {
    if (nfcActive) {
      STDOUT.println("-- Deactivating NFC (sleep mode)");
      
      nfcActive = false;
      tagCurrentlyPresent = false;
    }
  }
  
  void processNFC() {
    uint32_t now = millis();
    if(now - lastCheckTime < 500) {
      return;
    }
    lastCheckTime = now;

    i2cbus.inited();
    
    uint8_t uid[7];
    uint8_t uidLength;
    
    bool tagPresent = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
    
    if(tagPresent) {
      bool isDifferentTag = isNewTag(uid, uidLength);
      
      if(isDifferentTag) {
        STDOUT.println("-- New Crystal Detected");
        
        memcpy(lastUID, uid, uidLength);
        lastUIDLength = uidLength;
        tagCurrentlyPresent = true;
        
        if(readNFCData()) {
          applyNFCSettings();
          activateCrystalLED(6000);
        }
      }
      
    } else {
      if(tagCurrentlyPresent) {
        STDOUT.println("-- Crystal Removed");
        tagCurrentlyPresent = false;
      }
    }
  }
  
  void activateCrystalLED(int durationMs) {
    uint32_t r16 = (uint32_t)nfcColor[0] * 257;
    uint32_t g16 = (uint32_t)nfcColor[1] * 257;
    uint32_t b16 = (uint32_t)nfcColor[2] * 257;
    
    STDOUT.println("-- Activating crystal LED");
    
    if (current_config && current_config->blade2) {

      // Guardamos el estilo viejo del cristal.
      if (!savedCrystalStyle_) {
        savedCrystalStyle_ = current_config->blade2->UnSetStyle();
      }
      
      // Creamos y aplicamos el estilo encendido pulsante.
      if (crystalStyle_) {
        delete crystalStyle_;
        crystalStyle_ = nullptr;
      }
      crystalStyle_ = new CrystalLEDStyle(r16, g16, b16, durationMs);
      current_config->blade2->SetStyle(crystalStyle_);
      
      crystalLEDOn = true;
      crystalLEDOnTime = millis();
      
      STDOUT.println("-- Crystal LED active");
    }
  }

  void deactivateCrystalLED() {
    if (crystalStyle_ && current_config && current_config->blade2) {
      STDOUT.println("-- Deactivating crystal LED");
      
      // Quitarmos el estilo personalizado.
      current_config->blade2->UnSetStyle();
      
      // Restauramos el estilo original salvado.
      if (savedCrystalStyle_) {
        current_config->blade2->SetStyle(savedCrystalStyle_);
        savedCrystalStyle_ = nullptr;
      }
      
      delete crystalStyle_;
      crystalStyle_ = nullptr;
      crystalLEDOn = false;
      
      STDOUT.println("-- Crystal LED off");
    }
  }
  
  bool isNewTag(uint8_t* uid, uint8_t uidLength) {
    if(uidLength != lastUIDLength) {
      return true;
    }
    
    for(uint8_t i = 0; i < uidLength; i++) {
      if(uid[i] != lastUID[i]) {
        return true;
      }
    }
    
    return false;
  }
  
  bool readNFCData() {
    uint8_t pageBuffer[4];
    uint8_t decodedData[12];

    const uint8_t firma[12] = { 0x41, 0x72, 0x6B, 0x61, 0x69, 0x76, 0x6F, 0x73,
                             0x4B, 0x79, 0x62, 0x72 };

    for(uint8_t page = 4; page <= 6; page++) {
        bool success = nfc->ntag2xx_ReadPage(page, pageBuffer);
        if(!success) {
            STDOUT.print("! Error reading crystal ");
            STDOUT.println(page);
            strcpy(nfcPresetName, DEFAULT_PRESET_NAME);
            return false;
        }

        for(uint8_t i = 0; i < 4; i++) {
            decodedData[(page-4)*4 + i] = pageBuffer[i] ^ firma[(page-4)*4 + i];
        }
    }

    nfcColor[0] = decodedData[0];
    nfcColor[1] = decodedData[1];
    nfcColor[2] = decodedData[2];
    uint8_t presetNameLength = decodedData[3];
    if (presetNameLength > 8) presetNameLength = 8;

    if (presetNameLength != 0) {
        memcpy(nfcPresetName, decodedData + 4, presetNameLength);
        nfcPresetName[presetNameLength] = '\0';
    } else {
        strcpy(nfcPresetName, DEFAULT_PRESET_NAME);
    }

    for (uint8_t i = 0; i < presetNameLength; i++) {
        if (nfcPresetName[i] < 32 || nfcPresetName[i] > 126) {
            STDOUT.println("! Invalid preset name characters");
            strcpy(nfcPresetName, DEFAULT_PRESET_NAME);
            break;
        }
    }

    return true;
  }

  int findPresetByName(const char* presetName) {
    for (size_t i = 0; i < current_config->num_presets; i++) {
      if (strcmp(current_config->presets[i].name, presetName) == 0) {
        STDOUT.println(String("-- Found preset '") + presetName + "' at index " + i);
        return i;
      }
    }
    
    STDOUT.print("! Preset '");
    STDOUT.print(presetName);
    STDOUT.println("' not found, using default (0)");
    return 0;
  }
  
  void applyNFCSettings() {
    int targetPreset = findPresetByName(nfcPresetName);

    uint32_t r16 = (uint32_t)nfcColor[0] * 257;
    uint32_t g16 = (uint32_t)nfcColor[1] * 257;
    uint32_t b16 = (uint32_t)nfcColor[2] * 257;

    STDOUT.println(String("-- Applying color: RGB(") + nfcColor[0] + "," + nfcColor[1] + "," + nfcColor[2] + ")");
    STDOUT.println(String("-- Target preset: ") + targetPreset + " (" + nfcPresetName + ")");

    SetPreset(targetPreset, false);
    
    char styleString[128];
    snprintf(styleString, sizeof(styleString), "builtin %d 1 %lu,%lu,%lu", 
             targetPreset, r16, g16, b16);
    
    STDOUT.println(String("-- Style: ") + styleString);
    
    current_preset_.SetStyle(MAIN_BLADE, LSPtr<char>(mkstr(styleString)));
    
    STDOUT.println("-- Saving to presets.ini...");
    current_preset_.Save();
    
    #ifdef ENABLE_SD
    File f = LSFS::OpenForWrite("cur_kyber.txt");
    if (f) {
      uint8_t buffer[16];
      int len = snprintf((char*)buffer, sizeof(buffer), "%d", targetPreset);
      f.write(buffer, len);
      f.close();
      STDOUT.println(String("-- Saved current preset: ") + targetPreset);
    }
    #endif
    
    delay(200);
    
    SetPreset(targetPreset, true);

    STDOUT.println(String("-- Crystal Bonded (") + nfcColor[0] + "," + nfcColor[1] + "," + nfcColor[2] + ")  Preset: " + nfcPresetName);
  }
};

#endif