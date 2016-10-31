#include "pok3r.h"
#include "zlog.h"
#include "zfile.h"

int readver(){
    LOG("Looking for Vortex Pok3r...");
    Pok3r pok3r;
    if(pok3r.findPok3r()){
        if(pok3r.open()){
            LOG("Found: " << pok3r.getVersion());
            return 0;
        } else {
            LOG("Failed to Open");
            return -2;
        }
    } else {
        LOG("Not Found");
        return -1;
    }
}

int readfw(){
    LOG("Looking for Vortex Pok3r...");
    Pok3r pok3r;
    if(pok3r.findPok3r()){
        if(pok3r.open()){
            LOG("Reading...");
            ZBinary data;
            for(zu64 a = FW_ADDR; a < FW_ADDR + FW_LEN; a += 64){
                if(pok3r.read(a, data) != 64){
                    LOG("Failed to read");
                    return -3;
                }
            }
            LOG("Size: " << data.size());
            RLOG(data.dumpBytes(4, 8, true, FW_ADDR));
            return 0;
        } else {
            LOG("Failed to Open");
            return -2;
        }
    } else {
        LOG("Not Found");
        return -1;
    }
}

void fwDecode(ZBinary &bin){
    // Swap bytes 4 apart, skip 5
    for(zu64 i = 4; i < bin.size(); i+=5){
        zbyte a = bin[i-4];
        zbyte b = bin[i];
        bin[i-4] = b;
        bin[i] = a;
    }

    // Swap bytes in each set of two bytes
    for(zu64 i = 1; i < bin.size(); i+=2){
        zbyte d = bin[i-1];
        zbyte b = bin[i];
        bin[i-1] = b;
        bin[i] = d;
    }

    // y = (x - 7 << 4) + (x >> 4)
    for(zu64 i = 0; i < bin.size(); ++i){
        bin[i] = ((bin[i] - 7) << 4) + (bin[i] >> 4);
    }
}

void fwEncode(ZBinary &bin){
    // x = (y >> 4 + 7 & 0xF) | (x << 4)
    for(zu64 i = 0; i < bin.size(); ++i){
        bin[i] = (((bin[i] >> 4) + 7) & 0xF) | (bin[i] << 4);
    }

    // Swap bytes in each set of two bytes
    for(zu64 i = 1; i < bin.size(); i+=2){
        zbyte d = bin[i-1];
        zbyte b = bin[i];
        bin[i-1] = b;
        bin[i] = d;
    }

    // Swap bytes 4 apart, skip 5
    for(zu64 i = 4; i < bin.size(); i+=5){
        zbyte a = bin[i-4];
        zbyte b = bin[i];
        bin[i-4] = b;
        bin[i] = a;
    }
}

#define FWU_START       0x1A3800
#define FWU_LEN         0x64D4
#define STRINGS_START   0x1A9CD4
#define STRINGS_LEN     0x4B8

int decfw(ZPath exe){
    LOG("Extract from " << exe);
    ZFile file;
    if(!file.open(exe, ZFile::READ)){
        ELOG("Failed to open file");
        return -1;
    }

    // Read strings
    ZBinary strs;
    if(file.seek(STRINGS_START) != STRINGS_START){
        LOG("File too short");
        return -4;
    }
    if(file.read(strs, STRINGS_LEN) != STRINGS_LEN){
        LOG("File too short");
        return -5;
    }
    // Decrypt strings
    fwDecode(strs);

    ZString company;
    ZString product;
    ZString version;

    // Company name
    company.parseUTF16((const zu16 *)(strs.raw() + 0x10), 0xFF);
    // Product name
    product.parseUTF16((const zu16 *)(strs.raw() + 0x218), 0xFF);
    // Version
    version = ZString(strs.raw() + 0x460, 0xFF);

    LOG("Company: " << company);
    LOG("Product: " << product);
    LOG("Version: " << version);

    //RLOG(strs.dumpBytes(4, 8, true));

    // Read firmware
    ZBinary fw;
    if(file.seek(FWU_START) != FWU_START){
        LOG("File too short");
        return -2;
    }
    if(file.read(fw, FWU_LEN) != FWU_LEN){
        LOG("File too short");
        return -3;
    }

    //RLOG(fw.dumpBytes(4, 8, true));

    // Decrypt firmware
    fwDecode(fw);
    // Write firmware
    ZFile fwout("dump_" + version + ".bin", ZFile::WRITE);
    fwout.write(fw);

    RLOG(fw.dumpBytes(4, 8, true));

    return 0;
}

int encfw(ZPath exe, ZPath fw){
    LOG("Updater: " << exe);
    LOG("Decoded Firmware: " << fw);

    ZBinary exebin;
    if(!ZFile::readBinary(exe, exebin)){
        ELOG("Failed to read file");
        return -1;
    }

    ZFile fwfile;
    if(!fwfile.open(fw, ZFile::READ)){
        ELOG("Failed to open file");
        return -1;
    }
    // Read firmware
    ZBinary fwbin;
    if(fwfile.read(fwbin, FWU_LEN) != FWU_LEN){
        LOG("File too short");
        return -3;
    }

    // Encode firmware
    fwEncode(fwbin);

    // Write encoded firmware onto exe
    exebin.seek(FWU_START);
    exebin.write(fwbin);

    ZPath exeout = "patched.exe";
    ZFile exefile;
    if(!exefile.open(exeout, ZFile::WRITE)){
        ELOG("Failed to open file");
        return -3;
    }
    // Write updated exe
    if(!exefile.write(exebin)){
        LOG("Write error");
        return -4;
    }

    LOG("Updated Updater: " << exeout);

    return 0;
}

int main(int argc, char **argv){
    ZLog::logLevelStdOut(ZLog::INFO, "%time% %thread% N %log%");
    ZLog::logLevelStdErr(ZLog::ERRORS, "\x1b[31m%time% %thread% E [%file%:%line%] %log%\x1b[m");

    if(argc > 1){
        ZString cmd = argv[1];
        if(cmd == "readver"){
            return readver();
        } else if(cmd == "readfw"){
            return readfw();
        } else if(cmd == "decfw"){
            if(argc > 2){
                return decfw(ZString(argv[2]));
            } else {
                LOG("Usage: pok3rtest decfw <path to updater>");
                return 2;
            }
        } else if(cmd == "encfw"){
            if(argc > 3){
                return encfw(ZString(argv[2]), ZString(argv[3]));
            } else {
                LOG("Usage: pok3rtest encfw <path to updater> <path to firmware>");
                return 2;
            }
        } else {
            LOG("Unknown Command \"" << cmd << "\"");
            return 1;
        }
    } else {
        LOG("No Command");
        return 1;
    }
}
