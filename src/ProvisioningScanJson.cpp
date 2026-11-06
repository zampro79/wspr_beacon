#include "ProvisioningScanJson.h"

#include <stdio.h>
#include <string.h>

namespace {
class Writer {
 public:
  Writer(char *data,size_t capacity):data_(data),capacity_(capacity){if(capacity_)data_[0]='\0';}
  bool text(const char *value){return bytes(value,strlen(value));}
  bool bytes(const char *value,size_t length){if(!ok_||used_+length>=capacity_){ok_=false;return false;}memcpy(data_+used_,value,length);used_+=length;data_[used_]='\0';return true;}
  bool number(int32_t value){char b[16];const int n=snprintf(b,sizeof(b),"%ld",static_cast<long>(value));return n>0&&bytes(b,static_cast<size_t>(n));}
  bool unsignedNumber(uint32_t value){char b[16];const int n=snprintf(b,sizeof(b),"%lu",static_cast<unsigned long>(value));return n>0&&bytes(b,static_cast<size_t>(n));}
  bool string(const char *value){
    if(!text("\""))return false;
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(value);*p;++p){
      if(*p=='\"'||*p=='\\'){char escaped[2]={'\\',static_cast<char>(*p)};if(!bytes(escaped,2))return false;}
      else if(*p<0x20U){char escaped[7];const int n=snprintf(escaped,sizeof(escaped),"\\u%04x",*p);if(n!=6||!bytes(escaped,6))return false;}
      else if(!bytes(reinterpret_cast<const char*>(p),1))return false;
    }
    return text("\"");
  }
  bool ok()const{return ok_;} size_t size()const{return used_;}
 private: char *data_;size_t capacity_;size_t used_=0;bool ok_=true;
};
const char *stateName(WifiScanState state){switch(state){case WifiScanState::IDLE:return "IDLE";case WifiScanState::SCANNING:return "SCANNING";case WifiScanState::READY:return "READY";case WifiScanState::FAILED:return "FAILED";}return "FAILED";}
const char *errorName(WifiScanError error){switch(error){case WifiScanError::NONE:return "NONE";case WifiScanError::START_FAILED:return "START_FAILED";case WifiScanError::TIMEOUT:return "TIMEOUT";case WifiScanError::DRIVER_FAILED:return "DRIVER_FAILED";case WifiScanError::RECOVERY_EXHAUSTED:return "RECOVERY_EXHAUSTED";case WifiScanError::AP_RESTORE_FAILED:return "AP_RESTORE_FAILED";}return "UNKNOWN";}
}

bool ProvisioningScanJson::serialize(const WifiScanSnapshot &snapshot,
    uint32_t nowMillis,char *destination,size_t capacity,size_t &length){
  length=0; Writer out(destination,capacity);
  out.text("{\"schema\":1,\"state\":");out.string(stateName(snapshot.state));
  out.text(",\"generation\":");out.unsignedNumber(snapshot.generation);
  out.text(",\"age_ms\":");
  if(snapshot.completedTimestampValid)out.unsignedNumber(static_cast<uint32_t>(nowMillis-snapshot.completedMillis));else out.text("null");
  out.text(",\"stale\":");out.text(snapshot.state==WifiScanState::READY&&snapshot.completedTimestampValid&&static_cast<uint32_t>(nowMillis-snapshot.completedMillis)>=WifiScanController::STALE_MS?"true":"false");
  out.text(",\"driver_busy\":");out.text(snapshot.driverBusy?"true":"false");
  out.text(",\"recovery_blocked\":");out.text(snapshot.recoveryBlocked?"true":"false");
  out.text(",\"ap_restored\":");out.text(snapshot.apRestored?"true":"false");
  out.text(",\"error\":");out.string(errorName(snapshot.error));
  out.text(",\"networks\":[");
  for(uint8_t i=0;i<snapshot.count;++i){if(i)out.text(",");out.text("{\"ssid\":");out.string(snapshot.entries[i].ssid);out.text(",\"rssi\":");out.number(snapshot.entries[i].rssi);out.text(",\"protected\":");out.text(snapshot.entries[i].encrypted?"true":"false");out.text("}");}
  out.text("]}\n");
  if(!out.ok()||out.size()>MAX_RESPONSE_BYTES){if(capacity)destination[0]='\0';return false;}
  length=out.size();return true;
}
