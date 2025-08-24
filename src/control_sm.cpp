#include "control_sm.h"
#include "config_store.h"
#include "rpm_rmt.h"
#include "trigger_input.h"
#include "cut_output.h"
#include "log_ring.h"
#include "pins.h"
#include "pwm_test.h"
#include "lock_guard.h"

static State st = State::IDLE; 
static uint32_t tEntry=0; 
static uint16_t lastCut=0; 
static bool armedEdge=false;
static uint32_t lastCutTime=0;        // Thời điểm cắt cuối cùng
static uint16_t holdoffRemainMs=0;    // Thời gian holdoff còn lại
static const char* cutReason="ok";    // Lý do cắt/không cắt

static uint16_t lookupCut(uint16_t rpm, const QSConfig &c){
  // manual
  if (c.mode==Mode::MANUAL) return c.manual_kill_ms;
  // auto (linear between bands)
  for (uint8_t i=0;i<c.map_count;i++){
    auto b=c.map[i];
    if (rpm>=b.rpm_lo && rpm<b.rpm_hi) return b.cut_ms;
  }
  // above last band: use last
  return c.map[c.map_count-1].cut_ms;
}

static void pushLog(uint16_t rpm, uint16_t cut, bool autoMode, bool bf, CutOutputSel sel, const char* why){
  LogItem it{}; it.ts_ms=millis(); it.rpm=rpm; it.cut_ms=cut; it.auto_mode=autoMode; it.backfire=bf; strncpy(it.out,(sel==CutOutputSel::IGN?"IGN":"INJ"),3); strncpy(it.reason, why, 7); LOGR::push(it);
}

void CTRL::begin(){ st=State::IDLE; tEntry=millis(); }

// ===== Debug functions =====
uint16_t CTRL::getCurrentRPM() { return RPM::get(); }
const char* CTRL::getCurrentState() {
  switch(st) {
    case State::IDLE: return "IDLE";
    case State::ARMED: return "ARMED";
    case State::CUT: return "CUT";
    case State::RECOVER: return "RECOVER";
    default: return "UNKNOWN";
  }
}
uint16_t CTRL::getLastCutMs() { return lastCut; }
uint16_t CTRL::getHoldoffRemainMs() { return holdoffRemainMs; }
bool CTRL::canCutNow() { 
  QSConfig cfg = CFG::get();
  uint16_t rpm = RPM::get();
  return (rpm >= cfg.rpm_min) && (st == State::IDLE);
}
const char* CTRL::getCutReason() { return cutReason; }

void CTRL::tick(){

  QSConfig cfg = CFG::get();

  // Update RPM helpers
  RPM::setPPR(cfg.ppr); 
  RPM::setScale(cfg.rpm_scale);
  const uint16_t rpm = RPM::get();
  
  // Software tick for PWM test generator
  PWMTEST::tick();

  switch(st){
    case State::IDLE:
      if (TRIG::pressed()) { 
        st=State::ARMED; 
        tEntry=millis(); 
        armedEdge=true; 
        cutReason="triggered";
      }
      break;

    case State::ARMED: {
      bool ok = (rpm >= cfg.rpm_min);
      if (!ok) { 
        cutReason="below_rpm_min";
        st=State::IDLE; 
        break; 
      }
      
      // proceed to CUT
      st=State::CUT; 
      tEntry=millis();
      cutReason="cutting";
      
      uint16_t cut = lookupCut(rpm, cfg);
      bool useIgn = (cfg.cut_output==CutOutputSel::IGN);
      bool bf = false;
      if (cfg.backfire_enabled && rpm >= cfg.backfire_min_rpm){
        bf = true; 
        useIgn = true; // force IGN to keep fuel flowing
        cut = min<uint16_t>(CUT_MS_MAX, (uint16_t)(cut + cfg.backfire_extra_ms));
      }
      cut = constrain(cut, CUT_MS_MIN, CUT_MS_MAX);
      
      // Do cut (non-blocking)
      CUT::pulse(useIgn? CutLine::IGN : CutLine::INJ, cut);
      lastCut = cut;
      lastCutTime = millis();
      pushLog(rpm, cut, cfg.mode==Mode::AUTO, bf, cfg.cut_output, "shift");
      st=State::RECOVER; 
      tEntry=millis();
    } break;

    case State::RECOVER: {
      uint32_t elapsed = millis() - tEntry;
      if (elapsed >= CFG::get().holdoff_ms) { 
        st=State::IDLE; 
        cutReason="ok";
        holdoffRemainMs = 0;
      } else {
        holdoffRemainMs = CFG::get().holdoff_ms - elapsed;
        cutReason="holdoff";
      }
      break;
    }
  }
}
