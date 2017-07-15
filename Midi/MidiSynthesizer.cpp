#include "MidiSynthesizer.h"

#include <thread>
#include <QDebug>

MidiSynthesizer::MidiSynthesizer()
{
    for (int i=0; i<129; i++) {
        intmSf.push_back(0);
    }

    eq = new Equalizer31BandFX(0);
    reverb = new ReverbFX(0);
}

MidiSynthesizer::~MidiSynthesizer()
{
    if (openned)
        close();

    // Fx ------------
    if (eq->isOn())
        eq->off();
    delete eq;

    if (reverb->isOn())
        reverb->off();
    delete reverb;

    // --------------

    sfFiles.clear();
    intmSf.clear();
}

bool MidiSynthesizer::open()
{
    if (openned)
        return true;


    BASS_Init(-1, 44100, 0, NULL, NULL);
    BASS_SetConfig(BASS_CONFIG_BUFFER, 100);

    DWORD flags = BASS_SAMPLE_FLOAT|BASS_MIDI_SINCINTER;
    stream = BASS_MIDI_StreamCreate(32, flags, 0);


    //BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 5);
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_NOBUFFER, 1);

    qDebug() << "Synth buffer " << BASS_GetConfig(BASS_CONFIG_BUFFER);

    auto concurentThreadsSupported = std::thread::hardware_concurrency();
    float nVoices = (concurentThreadsSupported > 1) ? 500 : 256;
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_VOICES, nVoices);

    setSfToStream();

    for (int sfInex : intmSf) {
        if (sfInex > 0) {
            setMapSoundfontIndex(intmSf);
            break;
        }
    }

    BASS_ChannelPlay(stream, false);

    //BASS_MIDI_StreamEvent(stream, 9, MIDI_EVENT_DRUMS, 1);
    for (int i=16; i<32; i++) {
        BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_DRUMS, 1);
    }


    // Set stream to Fx
    eq->setStreamHandle(stream);
    reverb->setStreamHandle(stream);


    openned = true;
    return true;
}

void MidiSynthesizer::close()
{
    if (!openned)
        return;


    eq->setStreamHandle(0);
    reverb->setStreamHandle(0);


    BASS_ChannelStop(stream);

    for (HSOUNDFONT f : synth_HSOUNDFONT)
        BASS_MIDI_FontFree(f);

    synth_HSOUNDFONT.clear();

    BASS_StreamFree(stream);
    BASS_Free();

    openned = false;
}

int MidiSynthesizer::outPutDevice()
{
    return BASS_GetDevice();
}

bool MidiSynthesizer::setOutputDevice(int dv)
{
    ourtDev = dv;

    if (openned)
        return BASS_SetDevice(dv);
}

void MidiSynthesizer::setSoundFonts(std::vector<std::string> &soundfonsFiles)
{
    this->sfFiles.clear();
    this->sfFiles = soundfonsFiles;

    if (openned)
        setSfToStream();
}

void MidiSynthesizer::setVolume(float vol)
{
    if (BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, vol)) {
        synth_volume = vol;
    }
}

float MidiSynthesizer::soundfontVolume(int sfIndex)
{
    if (sfIndex < 0 || sfIndex >= synth_HSOUNDFONT.size())
        return -1;

    return BASS_MIDI_FontGetVolume(synth_HSOUNDFONT[sfIndex]);
}

void MidiSynthesizer::setSoundfontVolume(int sfIndex, float sfvl)
{
    if (sfIndex < 0 || sfIndex >= synth_HSOUNDFONT.size())
        return;

    BASS_MIDI_FontSetVolume(synth_HSOUNDFONT[sfIndex], sfvl);
}

bool MidiSynthesizer::setMapSoundfontIndex(const std::vector<int> &intrumentSfIndex)
{
    intmSf.clear();
    intmSf = intrumentSfIndex;

    if (intrumentSfIndex.size() < 129 || synth_HSOUNDFONT.size() == 0 || !openned)
        return false;


    std::vector<BASS_MIDI_FONTEX> mFonts;

    // check use another sf
    for (int i=0; i<129; i++) {

        if (intmSf.at(i) <= 0)
            continue;

        if (intmSf.at(i) >= synth_HSOUNDFONT.size())
            continue;

        BASS_MIDI_FONTEX font;
        font.font = synth_HSOUNDFONT.at(intmSf.at(i));

        if (i < 128) {
            font.spreset = i;
            font.sbank = 0;
            font.dpreset = i;
            font.dbank = 0;
            font.dbanklsb = 0;
        }
        else {
            font.spreset = -1;
            font.sbank = 128;
            font.dpreset = -1;
            font.dbank = 128;
            font.dbanklsb = 0;
        }

        mFonts.push_back(font);
    }

    // defaut sf
    BASS_MIDI_FONTEX f;
    f.font = synth_HSOUNDFONT.at(0);
    f.spreset = -1;
    f.sbank = -1;
    f.dpreset = -1;
    f.dbank = 0;
    f.dbanklsb = 0;

    mFonts.push_back(f);


    // set to stream
    BASS_MIDI_StreamSetFonts(stream, mFonts.data(), mFonts.size()|BASS_MIDI_FONT_EX);

    return true;
}

void MidiSynthesizer::sendNoteOff(int ch, int note, int velocity)
{
    if (note < 0 || note > 127)
        return;

    if (ch == 9)
        BASS_MIDI_StreamEvent(stream, getDrumChannelFromNote(note), MIDI_EVENT_NOTE, note);
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_NOTE, note);
}

void MidiSynthesizer::sendNoteOn(int ch, int note, int velocity)
{
    if (note < 0 || note > 127)
        return;

    if (ch == 9)
        BASS_MIDI_StreamEvent(stream, getDrumChannelFromNote(note), MIDI_EVENT_NOTE, MAKEWORD(note, velocity));
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_NOTE, MAKEWORD(note, velocity));
}

void MidiSynthesizer::sendNoteAftertouch(int ch, int note, int value)
{
    if (note < 0 || note > 127)
        return;

    if (ch == 9)
        BASS_MIDI_StreamEvent(stream, getDrumChannelFromNote(note), MIDI_EVENT_KEYPRES, MAKEWORD(note, value));
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_KEYPRES, MAKEWORD(note, value));
}

void MidiSynthesizer::sendController(int ch, int number, int value)
{
    DWORD et;

    switch (number) {
    case 0:
        et = MIDI_EVENT_BANK; break;
    case 1:
        et = MIDI_EVENT_MODULATION; break;
    case 5:
        et = MIDI_EVENT_PORTATIME; break;
    case 7:
        et = MIDI_EVENT_VOLUME; break;
    case 10:
        et = MIDI_EVENT_PAN; break;
    case 11:
        et = MIDI_EVENT_EXPRESSION; break;
    case 32:
        et = MIDI_EVENT_BANK_LSB; break;
    case 64:
        et = MIDI_EVENT_SUSTAIN; break;
    case 65:
        et = MIDI_EVENT_PORTAMENTO; break;
    case 66:
        et = MIDI_EVENT_SOSTENUTO; break;
    case 67:
        et = MIDI_EVENT_SOFT; break;
    case 71:
        et = MIDI_EVENT_RESONANCE; break;
    case 72:
        et = MIDI_EVENT_RELEASE; break;
    case 73:
        et = MIDI_EVENT_ATTACK; break;
    case 74:
        et = MIDI_EVENT_CUTOFF; break;
    case 75:
        et = MIDI_EVENT_DECAY; break;
    case 84:
        et = MIDI_EVENT_PORTANOTE; break;
    case 91:
        et = MIDI_EVENT_REVERB; break;
    case 93:
        et = MIDI_EVENT_CHORUS; break;
    case 94:
        et = MIDI_EVENT_USERFX; break;
    case 120:
        et = MIDI_EVENT_SOUNDOFF; break;
    case 121:
        et = MIDI_EVENT_RESET; break;
    case 123:
        et = MIDI_EVENT_NOTESOFF; break;
    case 126:
    case 127:
        et = MIDI_EVENT_MODE; break;
    default:
        return;
        break;
    }

    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, et, value);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, et, value);
}

void MidiSynthesizer::sendProgramChange(int ch, int number)
{
    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_PROGRAM, number);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_PROGRAM, number);
}

void MidiSynthesizer::sendChannelAftertouch(int ch, int value)
{
    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_CHANPRES, value);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_CHANPRES, value);
}

void MidiSynthesizer::sendPitchBend(int ch, int value)
{
    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_PITCH, value);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_PITCH, value);
}

void MidiSynthesizer::sendAllNotesOff(int ch)
{
    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_NOTESOFF, 0);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_NOTESOFF, 0);
}

void MidiSynthesizer::sendAllNotesOff()
{
    for (int i=0; i<16; i++) {
        sendAllNotesOff(i);
    }
}

void MidiSynthesizer::sendResetAllControllers(int ch)
{
    if (ch == 9) {
        for (int i=16; i<32; i++) {
            BASS_MIDI_StreamEvent(stream, i, MIDI_EVENT_RESET, 0);
        }
    }
    else
        BASS_MIDI_StreamEvent(stream, ch, MIDI_EVENT_RESET, 0);
}

void MidiSynthesizer::sendResetAllControllers()
{
    for (int i=0; i<16; i++) {
        sendResetAllControllers(i);
    }
}

std::vector<std::string> MidiSynthesizer::audioDevices()
{
    std::vector<std::string> dvs;

    int a, count=0;
    BASS_DEVICEINFO info;
    for (a=0; BASS_GetDeviceInfo(a, &info); a++)
        if (info.flags&BASS_DEVICE_ENABLED) { // device is enabled
            dvs.push_back(info.name);
            count++; // count it
        }

    return dvs;
}

bool MidiSynthesizer::isSoundFontFile(std::string sfile)
{
    bool result = true;

    HSOUNDFONT f = BASS_MIDI_FontInit(sfile.data(), 0);

    if (!f)
        result = false;

    BASS_MIDI_FontFree(f);

    return result;
}

void MidiSynthesizer::setSfToStream()
{
    for (HSOUNDFONT f : synth_HSOUNDFONT) {
        BASS_MIDI_FontFree(f);
    }
    synth_HSOUNDFONT.clear();

    for (const std::string &sfile : sfFiles) {
        HSOUNDFONT f = BASS_MIDI_FontInit(sfile.data(),
                                          BASS_MIDI_FONT_MMAP);
        if (f) {
            synth_HSOUNDFONT.push_back(f);
        }
    }

    if (synth_HSOUNDFONT.size() > 0) {
        BASS_MIDI_FONT font;
        font.font = synth_HSOUNDFONT.at(0);
        font.preset = -1;
        font.bank = 0;

        //BASS_MIDI_StreamSetFonts(0, &font, 1); // set sf to default stream
        BASS_MIDI_StreamSetFonts(stream, &font, 1); // set to stream to
    }

    // Reset map intrument sf
    intmSf.clear();
    for (int i=0; i<129; i++) {
        intmSf.push_back(0);
    }
}

int MidiSynthesizer::getDrumChannelFromNote(int drumNote)
{
    int ch = 0;
    switch (drumNote) {

    // Bass drum
    case 35:
    case 36: ch = 16; break;

    // snare
    case 38:
    case 40: ch = 17; break;

    // Side Stick/Rimshot
    case 37: ch = 18; break;

    // Low Tom
    case 41:
    case 43: ch = 19; break;

    // Mid Tom
    case 45:
    case 47: ch = 20; break;

    // High Tom
    case 48:
    case 50: ch = 21; break;

    // Hi-hat
    case 42:
    case 44:
    case 46: ch = 22; break;

    // Cowbell
    case 56: ch = 23; break;

    // Crash Cymbal ( 55 = Splash Cymbal )
    case 49:
    case 55:
    case 57: ch = 24; break;

    // Ride Cymbal
    case 51:
    case 59: ch = 25; break;

    // Bongo
    case 60:
    case 61: ch = 26; break;

    // Conga
    case 62:
    case 63:
    case 64: ch = 27; break;

    // Timbale
    case 65:
    case 66: ch = 28; break;

    // ฉิ่ง
    case 80:
    case 81: ch = 29; break;

    // Chinese Cymbal ( ฉาบ )
    case 52: ch = 30; break;

    default:
        ch = 31;
        break;
    }

    return ch;
}
