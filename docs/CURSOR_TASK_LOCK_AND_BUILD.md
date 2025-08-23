# Cursor Work Order — Default Wi-Fi SSID + Fix "Tắt Lock" + Pass Mode Toggle
**Project:** ESP32-C3 Quickshifter (PlatformIO + LittleFS + AsyncWebServer)

## Mục tiêu
1) Đặt **SSID mặc định** theo MAC:
   ```cpp
   // YÊU CẦU (giữ đúng công thức của user):
   String ssid = String("QS-TuLamDienTu-") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
(Có sanitize/đệm ‘0’ cho an toàn, ≤ 31 ký tự.)
2) Sửa lỗi “Tắt Lock (không cần pass)”: khi nhấn tắt thì:

lock_enabled=false (persist),

locked=false, nhả cắt ngay (IGN/INJ=LOW),

reset state (seq/timers/retries), không còn đọc NPN 0/1.

Thêm nút gạt/checkbox trên UI để bật/tắt chế độ pass (lock_enabled) trực quan.

Tắt: gọi API disable.

Bật: set lock_enabled=true vào config (không auto-lock; “Lock Now” vẫn hoạt động).

Phạm vi file
src/config_store.cpp, src/config_store.h

src/web_ui.cpp, src/web_ui.h

src/lock_guard.cpp, src/lock_guard.h

data/index.html

Công việc chi tiết
A) SSID mặc định theo MAC
Yêu cầu bắt buộc:

Nếu ap_ssid trống/không tồn tại → tạo mặc định theo công thức sau khi khởi tạo cấu hình hoặc lúc mở AP lần đầu:

cpp
Sao chép
Chỉnh sửa
// Giữ đúng công thức của user, nhưng thêm padding + uppercase để ổn định:
uint32_t mac32 = (uint32_t)ESP.getEfuseMac();
String macHex = String(mac32, HEX); macHex.toUpperCase();
while (macHex.length() < 8) macHex = "0" + macHex;          // đệm đến 8 ký tự
String ssid = String("QS-TuLamDienTu-") + macHex.substring(4); // lấy 4 ký tự cuối như yêu cầu
// clamp ≤ 31 ký tự (SSID giới hạn 32 byte), ở đây chuỗi luôn < 31 nên OK.
Ghi ssid vào CFG nếu trước đó rỗng. Không thay đổi nếu user đã đặt ssid trước.

Điểm chèn code:

Trong CFG::begin() (sau khi load prefs) hoặc trong WEB::beginPortal() nếu CFG::get().ap_ssid rỗng → tạo như trên rồi CFG::set(c).

B) Sửa lỗi “Tắt Lock (không cần pass)”
Server:

Mở rộng endpoint POST /api/lock_cmd case "disable":

cpp
Sao chép
Chỉnh sửa
if (cmd == "disable") {
  auto c = CFG::get(); 
  c.lock_enabled = false;
  CFG::set(c);                 // persist

  // Nhả cắt + reset state:
  CUT::set(CutLine::IGN, false);
  CUT::set(CutLine::INJ, false);
  LOCK::reset();               // seq="", timers, retries=0; locked=false
  // Đảm bảo locked=false:
  LOCK::forceUnlockImmediate(); // nếu chưa có, tạo helper đặt locked=false & nhả cắt

  SLOGln("[LOCK] disabled via API");
  return ok(req, "disabled");
}
Đảm bảo sau khi disable:

LOCK::isLocked() == false

CFG::get().lock_enabled == false

LOCK::tick() không còn đọc TRIG::rawLevel() (đọc raw chỉ khi lock_enabled && locked).

Lock guard:

Trong LOCK::tick():

cpp
Sao chép
Chỉnh sửa
if (!CFG::get().lock_enabled || !g_locked) return; // bỏ qua NPN pass-mode khi đã disable hoặc unlock
Thêm helper (nếu thiếu):

cpp
Sao chép
Chỉnh sửa
void LOCK::forceUnlockImmediate() {
  g_locked = false;
  CUT::set(CutLine::IGN,false);
  CUT::set(CutLine::INJ,false);
  resetState(); // seq="", timers, retries=0
}
C) UI — Toggle bật/tắt pass
index.html (Tab Lock): thêm switch/checkbox:

html
Sao chép
Chỉnh sửa
<label class="switch">
  <input type="checkbox" id="lockEnabled">
  <span class="slider"></span>
</label>
<span>Chế độ pass (Lock enabled)</span>

<!-- Nút Tắt Lock hiện có vẫn giữ lại, nhưng toggle này là cách chính -->
<button id="btnLockDisable" class="secondary">Tắt Lock (không cần pass)</button>
CSS tối giản (nếu chưa có switch):

css
Sao chép
Chỉnh sửa
.switch{position:relative;display:inline-block;width:48px;height:24px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#888;transition:.2s;border-radius:24px}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background:#fff;transition:.2s;border-radius:50%}
input:checked + .slider{background:#2e7d32}
input:checked + .slider:before{transform:translateX(24px)}
JS:

js
Sao chép
Chỉnh sửa
async function refreshLockState(){
  const r = await fetch("/api/lock_state",{cache:"no-store"});
  if (!r.ok) return toast(false,"Lock state failed");
  const j = await r.json();
  q("#lockEnabled").checked = !!j.enabled;
  // … hiển thị locked/cut nếu muốn
}

// Toggle lock_enabled
q("#lockEnabled").onchange = async (e)=>{
  const enabled = e.target.checked;
  if (!enabled) {
    // call disable API -> persist false, unlock, nhả cắt
    const r = await fetch("/api/lock_cmd", {
      method:"POST",
      headers:{"Content-Type":"application/json"},
      cache:"no-store",
      body:JSON.stringify({cmd:"disable"})
    });
    toast(r.ok, r.ok?"Lock disabled":"Disable failed");
  } else {
    // enable in config (không auto-lock)
    const cfg = await (await fetch("/api/config/get",{cache:"no-store"})).json();
    cfg.lock_enabled = true;
    const s = await fetch("/api/config/set", {
      method:"POST",
      headers:{"Content-Type":"application/json"},
      cache:"no-store",
      body: JSON.stringify(cfg)
    });
    toast(s.ok, s.ok?"Lock enabled":"Enable failed");
  }
  await refreshLockState();
};

// Nút “Tắt Lock (không cần pass)” giữ nguyên, gọi disable:
q("#btnLockDisable").onclick = async ()=>{
  const r = await fetch("/api/lock_cmd",{
    method:"POST",
    headers:{"Content-Type":"application/json"},
    cache:"no-store",
    body:JSON.stringify({cmd:"disable"})
  });
  toast(r.ok, r.ok?"Lock disabled":"Disable failed");
  await refreshLockState();
};
Lưu ý: không auto-lock khi bật lại lock_enabled. User vẫn có nút “Lock Now” riêng (nếu có) để chuyển sang trạng thái locked.

Bảo mật & Sanitize
ap_ssid ≤ 31 ký tự; lọc control chars (< 0x20).

ap_pass 8–63 ký tự; nếu POST /api/wifi/config pass < 8 → 400.

json/export mặc định ẩn pass: "ap_pass":"***" (chỉ lộ khi ?include_secret=1).

lock_code chỉ 0|1, dài ≤ 8 (sanitize trong import/set).

Acceptance
1) SSID mặc định
Xóa ap_ssid trong prefs (hoặc trên thiết bị mới), khởi động:

/api/wifi/state.ssid khớp mẫu QS-TuLamDienTu-xxxx.

Dài chuỗi ≤ 31, không ký tự lạ.

2) “Tắt Lock (không cần pass)” FIX
Đang locked=true, bấm Tắt Lock:

HTTP 200 "disabled".

/api/lock_state.enabled=false, /api/lock_state.locked=false.

IGN/INJ = LOW ngay.

NPN không còn bị đọc 0/1 (unlock xong không còn pass-mode).

Test RPM → QS hoạt động bình thường.

3) Toggle pass-mode (lock_enabled)
Bật switch:

lock_enabled=true (persist), không auto-lock.

Tắt switch:

Gọi disable API, lock_enabled=false, locked=false, nhả cắt, bỏ đọc NPN.

4) Không hồi quy
/api/get|set, /api/lock_cmd unlock, /api/lock_state vẫn chạy.

UI toasts hiển thị OK/Fail đúng trạng thái.

Gợi ý triển khai nhanh
config_store.cpp

Trong CFG::begin() sau khi load, nếu ap_ssid rỗng → tạo SSID theo công thức, CFG::set(c).

Sanitize ap_ssid, ap_pass.

web_ui.cpp

Trong beginPortal(): dùng CFG::get().ap_ssid & ap_pass hiện tại.

/api/lock_cmd thêm case "disable" như phần B.

Tất cả response thêm header Cache-Control: no-store, no-cache, must-revalidate.

lock_guard.cpp

LOCK::tick() ngay đầu:

cpp
Sao chép
Chỉnh sửa
if (!CFG::get().lock_enabled || !g_locked) return;
Thêm LOCK::forceUnlockImmediate() nếu chưa có.

Artefacts cần xuất
diff.patch (unified diff các file sửa).

build_log.txt (PlatformIO build OK).