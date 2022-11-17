// MIDIParserV5.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <string>
#include <array>
#include <algorithm>

void endProgram() {
    std::cerr << "Cannot parse this file" << "\n";
    std::cin.get();
    exit(0);
}

void dtToNotes(uint64_t dt, uint32_t tpqn) { 
    int a = 100000, b = dt;
    std::string fullTime = "";
    while (b > 0) { //while there is some delta time remnant left...
        a = floor(log2(b) - log2(tpqn)); //Find the largest possible x where TPQN*pow(2, x) <= DT

        if (a >= 2) { //If that value gives an integer note:
            fullTime = fullTime + std::to_string(uint32_t(pow(2, a - 2))) + "+";
        }
        else {//If that value gives a fractional note:
            fullTime = fullTime +  "1/" + std::to_string(uint32_t(pow(2, ((a * -1) + 2)))) + "+";
        }
        b -= (pow(2, a) * tpqn);//Subtract away pow(2, x) from DT, and then we can repeat the above process with the leftovers
    }
    if (fullTime.length() < 1) {
        std::cout << "length in notes: 0 notes\n";
    }else{
        fullTime = fullTime.substr(0, fullTime.size() - 1);
        std::cout << "length in notes: " <<fullTime << " notes\n";
    }
}

void mpqnToBpm(uint32_t mpqn) { //microseconds per quarter note --> BPM
    float bpm = 60000000 / mpqn; // bpm is always counted with the amount of quarter notes that occur in one minute
    std::cout << "Beats per minute : " << bpm << "\n";
}

double dtToBb(uint32_t tpqn, uint8_t denominator, uint32_t dt) { //delta times --> multiple of a beat (in accordance with the time signature) e.g. in 4/4 time, 1 = 1 quarter note duration, 1/2 = 1 eigth note duration etc
    double Bb = ((double(pow(2, denominator)) / 4.00f) * (double(dt) / double(tpqn)));
    std::cout << dt << " in ticks is equivalent to " << Bb << " beats\n"; //beats here are based on the time signature
    return Bb;
}

std::string noteTonum(uint8_t note, std::array<std::string, 12> availableNotes = { "C", "C Sharp", "D", "E Flat", "E", "F","F Sharp", "G", "G Sharp","A", "B Flat", "B" }) {
    if (note == 0) {
        std::cout << "Rest" << "\n";
        return "R";
    }
    else {
        uint8_t octave = floor(note / 12) - 1;
        std::string noteValue = availableNotes[note % 12];
        std::string fullNote = noteValue + std::to_string(octave);
        std::cout << "Note value: " << fullNote << "\n";
        return fullNote;
    }
}

void velToDyn(uint8_t velocity, std::array<std::string, 10> availableDynamics = { "pppp", "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff", "ffff" }) {// velocity to dynamic
    std::string dynamic = availableDynamics[floor((velocity + 2) / 13)];
    std::cout << "Dynamic value: " << dynamic << "\n";

}

uint64_t noteTimeElapsed(uint64_t startTime, uint64_t endTime) { //Time Elapsed in delta time ticks
    return (endTime - startTime);
}

uint64_t quantize(uint64_t dt, uint32_t tpqn) {
    const int boundaryOfPrecision = -3;//In terms of QuarterNote * pow(2, boundaryOfPrecision), e.g. Quarternote * pow(2, -3) = 1/8 * QuarterNote = 1/32 note
    int ticksPerBoundary = tpqn * pow(2, boundaryOfPrecision);

    //Stop integer division by 0
    if (ticksPerBoundary == 0) {
        return dt;
    }

    //Quantize values if they need quantizing
    if (dt % ticksPerBoundary != 0) {
        if ((dt % ticksPerBoundary) <= (ticksPerBoundary / 2)) { // Round down
            dt = dt - (dt % ticksPerBoundary);
        }
        else { //Round up
            dt = dt + (ticksPerBoundary - (dt % ticksPerBoundary));
        }
    }
    return dt;
}

//If we assume the time signature to be 4/4, this means that there are 4, 1/4 notes in a bar. AKA 4*TPQN ticks in a bar.
//n = numerator, r = denominator
//Likewise, if the signature is n/pow(2, r), there are n, 1/pow(2,r) notes in a bar. In 1/pow(2,r) notes there are 4/pow(2,r) 1/4 notes. So we get n*4*TPQN/pow(2,r) ticks in a bar
//For 1/2 notes, r = 1, For 1/4 notes, r = 0, For 1/8 notes, r = -1
//This means that we have a set number of ticks in a bar.
//If we add barlines in terms of ticks, we can use delta-time values to asses whether the barlines split up a note or not
uint32_t getBarlineTicks(uint8_t numerator, uint8_t denominator, uint32_t tpqn) {
    return (uint32_t(numerator) * 4 * tpqn) / pow(2, denominator);
}

enum midi_messageStatus {
    noteOn = 0x90,
    noteOff = 0x80,
    polyAftertouch = 0xA0,
    controlChange = 0xB0,
    programChange = 0xC0,
    monoAftertouch = 0xD0,
    pitchBend = 0xE0,
    sysEx = 0xf7,
    sysCommon = 0xf0,
    meta = 0xff
};

enum midi_meta_messageStatus {
    endOfTrack = 0x2f,
    setTempo = 0x51,
    timeSignature = 0x58,
    keySignature = 0x59,
    //instrumentName = 04
};
struct midi_header_info { //stores data on MIDI headers
    uint16_t fileFormat = 0;
    uint16_t trackNumber = 0;
    uint16_t division = 0;

};

struct midi_event {
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    bool barline = false;
    virtual void output(uint16_t tpqn) = 0;
};



struct midi_note : public midi_event {
    uint8_t note;
    uint8_t channel;
    uint8_t velocity;
    std::string state = "none";
    void output(uint16_t ticksPerQuarterNote) override {
        std::cout << "Note\n";
        std::cout << "Start time: " << startTime << "\n";
        //std::cout << "Channel: " << unsigned(channel) << "\n";
        //dtToNotes(i->startTime, ticksPerQuarterNote);
        //std::cout << "endTime: " << "\n";
        //dtToNotes(i->endTime,ticksPerQuarterNote) ;
        std::cout << "State: " << state << "\n";
        noteTonum(note);
        velToDyn(velocity);
        //std::cout << "Start time: " <<std::dec <<startTime << "\n";
        //std::cout << "End time: " << std::dec <<i->endTime << "\n";
        //std::cout << "Note time in beats: " << dtToBb(ticksPerQuarterNote,denominator, noteTimeElapsed(i->startTime, i->endTime)) << "\n";
        dtToNotes(noteTimeElapsed(startTime, endTime), ticksPerQuarterNote);
    }
};

struct midi_barline : public midi_event {
    uint32_t barNumber = 0;
    void output(uint16_t ticksPerQuarterNote) override{
        std::cout << "Barline\n";
        //std::cout << "cummulativeDeltaTime = " <<std::dec << startTime << "\n";
        std::cout << "Start of bar: " << barNumber << "\n";
    }
};

struct midi_rest : public midi_event {
    void output(uint16_t ticksPerQuarterNote) override {
        std::cout << "Rest\n";
        dtToNotes(noteTimeElapsed(startTime, endTime), ticksPerQuarterNote);
    }
};

struct midi_timeSignature : public midi_event {
    uint8_t numerator = 4;
    uint8_t denominator = 2;
    uint8_t clocksPerMetronomeTick = 0;
    uint8_t notesPerQuarterNote = 0; // in 32nd notes
    void output(uint16_t ticksPerQuarterNote) override {
        std::cout << "The time signature is: " << unsigned(numerator) << "/" << pow(2, denominator) << "\n";
        std::cout << "This time signature begins at delta time : " << startTime << "\n";
        dtToNotes(startTime, ticksPerQuarterNote);
    }
};

struct midi_keySignature : public midi_event {
    int8_t accidentalCount = 0;
    uint8_t minor = true;
    void output(uint16_t ticksPerQuarterNote) override {
        std::cout << "The number of sharps/flats (sharps are positive, flats are negative) are: " << int(accidentalCount) << "\n";
        std::cout << "The key is (1 = minor, 0 = major): " << unsigned(minor) << "\n";
    }
};

struct midi_tempo : public midi_event {
    uint32_t microsecondsPerQuarternote = 0;
    void output(uint16_t ticksPerQuarterNote) override {
        std::cout << "From delta time : " << startTime <<",\n";
        //std::cout << "which is"<< << "bars and " << <<"beats from the beginning of the piece";
        dtToNotes(startTime,ticksPerQuarterNote);
        mpqnToBpm(microsecondsPerQuarternote);
        //std::cout << "The number of microseconds per quarter note are: " << std::dec << microsecondsPerQuarternote << "\n";
    }
};

void readData_Error(std::ifstream& file_reader) { //check for file corrution
    if (!file_reader) { // filestream is corrupted
        std::cerr << "Error - file corruption" << "\n";
        endProgram();
    }
}

template<typename T> // only accepts integer types
void changeEndianness(T& data) { // uses bit shifting to swap each byte of data around
    T finalData = 0;

    for (int i = 0; i < sizeof(T); i++) {
        finalData = (finalData << 8) | (0xff & data);
        data = data >> 8;
        finalData << 8;
    }
    data = finalData;
}


template<typename T>
void readData(T& data, std::ifstream& file_reader) { //read sizeof(T) length data from the file, and convert it to a processable form
    file_reader.read(reinterpret_cast<char*>(&data), sizeof(T));
    readData_Error(file_reader);
    changeEndianness<T>(data);
}

uint32_t readVariableLengthQuantity(std::ifstream& file_reader) { //assuming VLQ's are 32 bits
    uint32_t result = 0;
    uint8_t data = 0x80;
    uint8_t check = 0;


    while (((0x80 & data) >> 7)) // while the first bit == 1 
    {
        file_reader.read(reinterpret_cast<char*>(&data), sizeof(data));
        check = (data & 0x7f); // mask off the first bit from data (we don't need it for VLQ's)
        result = ((result << 7) | check); //add the contents of the file to result

    }

    return result;
}



void readHeader_Output(uint32_t& midi_header_signature, uint32_t& midi_header_length, midi_header_info& data) {

    std::cout << "This chunk's signature is: " << std::hex << midi_header_signature << "\n";
    std::cout << "This chunk's length is: " << std::hex << midi_header_length << "\n";
    std::cout << "This file's format is: " << std::hex << data.fileFormat << "\n";
    std::cout << "This file has: " << std::hex << data.trackNumber << " tracks\n";
    std::cout << "This file uses a time division of: " << std::hex << data.division << "\n";
    std::cout << "\n";
}

void readHeader_Error(uint32_t& midi_header_signature, uint32_t& midi_header_length) {
    if (midi_header_signature != 'MThd') {
        std::cerr << "Error - chunk type incorrect" << "\n";
        endProgram();

    }

    if (midi_header_length != 6) {
        std::cerr << "Error - header length incorrect" << "\n";
        endProgram();
    }
}


void readHeader(std::ifstream& file_reader, midi_header_info& data) { //reading the header chunk
    uint32_t midi_header_signature = 0;
    uint32_t midi_header_length = 0;

    readData<uint32_t>(midi_header_signature, file_reader);
    readData<uint32_t>(midi_header_length, file_reader);
    readHeader_Error(midi_header_signature, midi_header_length);

    readData<uint16_t>(data.fileFormat, file_reader);
    readData<uint16_t>(data.trackNumber,     file_reader);
    readData<uint16_t>(data.division, file_reader);

    //readHeader_Output(midi_header_signature, midi_header_length, data);
}

void readStream_ErrorRecovery(std::ifstream& file_reader, uint32_t& midi_track_length) {
    uint8_t garbage = 0;
    for (int i = 0; i < midi_track_length; i++) {
        readData<uint8_t>(garbage, file_reader);
    }
}

void readStream_Error(uint32_t& midi_track_signature, uint32_t& midi_track_length, std::ifstream& file_reader, bool& endOfTrack) {
    if (midi_track_signature != 'MTrk') {
        std::cerr << "Error - chunk type " << std::hex << midi_track_signature << " unparseable" << "\n";
        readStream_ErrorRecovery(file_reader, midi_track_length);
        endOfTrack = true;
    }
}

void readStream_throwaway(std::ifstream& file_reader, uint32_t throwawayLength) {
    uint8_t garbage = 0;
    for (int i = 1; i <= throwawayLength; i++) {
        readData<uint8_t>(garbage, file_reader);
    }
}

void readStream_runningStatus(std::ifstream& file_reader, uint8_t& status, uint8_t& runningStatus) {

    //ERROR
    if (status < 0x80 && runningStatus < 0x80) { //if status is a data byte, and there is no running status
        std::cerr << "Error - data byte without preceding status byte. Bytestream corrupted" << "\n";
        //endProgram();
    }

    //SET RUNNING STATUS
    else if (status < 0x80 && runningStatus >= 0x80) { //if status is a data byte, and running status is present
        status = runningStatus;
        file_reader.seekg(-1, std::ios_base::cur);
    }

    //MAKE RUNNING STATUS THE LAST STATUS BYTE
    else {
        runningStatus = status;
    }

}

void newNote(std::vector<midi_note*>& unfinishedNotes, uint8_t note, uint8_t velocity, uint8_t channel, uint64_t cummulativeDeltaTime) {
    midi_note* newNote = new midi_note;

    newNote->startTime = cummulativeDeltaTime;
    //newNote->octave = std::stoi(convertedNote.substr(noteTonum(note).length() - 2,1));
    //newNote->pitch = convertedNote.substr(0, noteTonum(note).length() - 1);
    newNote->note = note;
    newNote->velocity = velocity;
    newNote->channel = channel;

    unfinishedNotes.push_back(newNote);
}

void endNote(std::vector<midi_note*>& unfinishedNotes, std::vector<midi_note*>& finishedNotes, uint8_t note, uint64_t cummulativeDeltaTime) {//This adds notes to a vector based on finishing time. But we want our results on starting time so we gotta sort
    int count = 0;
    for (std::vector<midi_note*>::iterator i = unfinishedNotes.begin(); i != unfinishedNotes.end(); i++) {
        if (note == (*i)->note) {
            (*i)->endTime = cummulativeDeltaTime;
            finishedNotes.push_back(*i);
            unfinishedNotes.erase(i);
            return;
        }
        count++;
    }
    //No corresponding starting note to the ending note, let's ignore this
}

void splitNotes(midi_note* noteUsed, std::vector<midi_event*>& splitNotes, uint32_t subtractedNoteDuration, uint32_t barlineTicks, int s1 = 0, int s2 = 1, int s3 = 2) {
    std::vector<std::string> stateMessages = {
        "tied to the note below","tied to the note below and above","tied to the note above"
    };
    if (noteUsed->endTime == 0) { //This is to prevent (ceil(double(i->endTime) / barlineTicks)-1 ) become -1 when i->endTime = 0, and cause bugs
            subtractedNoteDuration = 0;

    }
    else if ((noteUsed->endTime - noteUsed->startTime) % barlineTicks == 0) {
        subtractedNoteDuration = ((noteUsed->endTime - noteUsed->startTime)/barlineTicks);
    }
    else {
            subtractedNoteDuration = (ceil(double(noteUsed->endTime) / double(barlineTicks)) - 1) - floor(double(noteUsed->startTime) / double(barlineTicks));
    }

    if (subtractedNoteDuration > 0) {//If not in the same bar
        //One note which is the part of the note in the first bar --> startTime = startTime, endTime = floor(i->startTime/barlineTicks) * barLineTicks
        //One(or more) notes which are in the next bars --> startTime = floor(i->startTime/barlineTicks) * barLineTicks, endTime = either the end of the next bar, or more likely, i->endTime
        //There will be more that one note if the subtractedNoteDuration is bigger than 1 --> we can use this as a for loop counter
        int x = 1;
        //Create beginning note --> tied status
        midi_note* newNote = new midi_note;
        newNote->state = stateMessages[s1];
        newNote->note = noteUsed->note;
        newNote->channel = noteUsed->channel;
        newNote->velocity = noteUsed->velocity;
        newNote->startTime = noteUsed->startTime;
        newNote->endTime = (floor(noteUsed->startTime / barlineTicks) + x) * barlineTicks;

        splitNotes.push_back(newNote);


        while (x <= subtractedNoteDuration - 1) {
            //Create full note 
            newNote = new midi_note;
            newNote->state = stateMessages[s2];
            newNote->note = noteUsed->note;
            newNote->channel = noteUsed->channel;
            newNote->velocity = noteUsed->velocity;
            newNote->startTime = (floor(noteUsed->startTime / barlineTicks) + x) * barlineTicks;
            newNote->endTime = (floor(noteUsed->startTime / barlineTicks) + x + 1) * barlineTicks;
            splitNotes.push_back(newNote);
            x += 1;
        }


        //Create remainder note
        newNote = new midi_note;
        newNote->state = stateMessages[s3];
        newNote->note = noteUsed->note;
        newNote->channel = noteUsed->channel;
        newNote->velocity = noteUsed->velocity;
        newNote->startTime = ((floor(noteUsed->startTime / barlineTicks) + x) * barlineTicks);
        newNote->endTime = noteUsed->endTime;



        splitNotes.push_back(newNote);
    }
    else {//in same bar
        midi_note* newNote = new midi_note;
        newNote->state = "None";
        newNote->note = noteUsed->note;
        newNote->channel = noteUsed->channel;
        newNote->velocity = noteUsed->velocity;
        newNote->startTime = noteUsed->startTime;
        newNote->endTime = noteUsed->endTime;

        splitNotes.push_back(newNote);
    }
}

void outputEvents(uint32_t tpqn, midi_event* i) {
    std::cout << "------------------------------------------------" << "\n";
    i->output(tpqn);
    std::cout << "------------------------------------------------" << "\n";
    //std::cin.get();
}

void readStream_readEvent(std::ifstream& file_reader, bool endOfTrack, uint16_t ticksPerQuarterNote, std::vector<midi_timeSignature*>& timingEvents) {
    uint8_t runningStatus = 0; // holds the status byte for running status
    uint32_t deltaTime = 0;
    uint8_t status = 0;
    uint8_t tempStatus = 0;
    uint8_t channel = 0;
    uint64_t cummulativeDeltaTime = 0;

    //Note data streams
    std::vector<midi_note*> unfinishedNotes; //only incomplete notes 
    std::vector<midi_note*> finishedNotes; //completed notes only
    std::vector<midi_event*> finishedNotesSplit; //completed notes only

    std::vector<midi_event*> generalEvents; //notes and general events
    std::vector<midi_event*> finishedNotesAndRests; //notes, rests, and general events

    //We somehow have to add barlines?, or split up notes so the barlines fit
    //When each event appears, look at the cummulative delta time
    //If we add to the sorting lambda, we can make sure that barlines always come before other midi events - we just have to change the member variables

    //Add new note to signal the start of the track. This is an empty note and helps us in calculating the first rest
    //newNote(unfinishedNotes, 0, 0, 0, 0);
    //endNote(unfinishedNotes, finishedNotes, 0, 0);


    while (!endOfTrack) { //read a MIDI event
        deltaTime = readVariableLengthQuantity(file_reader);
        readData<uint8_t>(status, file_reader);

        cummulativeDeltaTime += deltaTime;

        //running status check
        readStream_runningStatus(file_reader, status, runningStatus);


        tempStatus = status & 0xf0; //first byte of status byte is useful for selecting messages

        //NOTE ON
        if (tempStatus == midi_messageStatus::noteOn) {

            channel = status & 0x0f;
            uint8_t note = 0;
            uint8_t velocity = 0;
            readData<uint8_t>(note, file_reader);
            readData<uint8_t>(velocity, file_reader);


            //std::cout << "Note on message" << "\n";
            //noteTonum(note);
            //velToDyn(velocity);

            //Assess message
            if (velocity == 0) {

                endNote(unfinishedNotes, finishedNotes, note, quantize(cummulativeDeltaTime, ticksPerQuarterNote));
            }
            else {
                newNote(unfinishedNotes, note, velocity, channel, quantize(cummulativeDeltaTime, ticksPerQuarterNote));
            }

        }

        //NOTE OFF
        else if (tempStatus == midi_messageStatus::noteOff) {

            channel = status & 0x0f;
            uint8_t note = 0;
            uint8_t velocity = 0;
            readData<uint8_t>(note, file_reader);
            readData<uint8_t>(velocity, file_reader);

            //std::cout << "Note off message" << "\n";
            //noteTonum(note);
            //velToDyn(velocity);


            //Assess message
            endNote(unfinishedNotes, finishedNotes, note, quantize(cummulativeDeltaTime, ticksPerQuarterNote));
        }

        //META
        else if (status == midi_messageStatus::meta) {

            //std::cout << "Meta message" << "\n";

            uint8_t metaMessageStatus = 0;

            readData(metaMessageStatus, file_reader);
            uint32_t eventLength = readVariableLengthQuantity(file_reader);

            if (metaMessageStatus == midi_meta_messageStatus::endOfTrack) {
                endOfTrack = true;
            }
            else if (metaMessageStatus == midi_meta_messageStatus::keySignature) {

                midi_keySignature* newKeySignature = new midi_keySignature;
                newKeySignature->startTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                newKeySignature->endTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                readData<int8_t>(newKeySignature->accidentalCount, file_reader);
                readData<uint8_t>(newKeySignature->minor, file_reader);
                //std::cout << unsigned(newKeySignature->minor) << "\n";

                generalEvents.push_back(newKeySignature);

                //std::cout << "The number of sharps/flats are: " << unsigned(accidentalCount) << "\n";
                //std::cout << "The key is (1 = minor, 0 = major): " << unsigned(minor) << "\n";
            }
            else if (metaMessageStatus == midi_meta_messageStatus::setTempo) {

                uint8_t temp = 0;
                midi_tempo* newTempo = new midi_tempo;

                newTempo->startTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                newTempo->endTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                //mpqn is 3 bytes long so it needs its own conversion algorithm
                for (int i = 0; i < 3; i++) {
                    readData<uint8_t>(temp, file_reader);
                    newTempo->microsecondsPerQuarternote = (newTempo->microsecondsPerQuarternote << 8) | temp;
                }
                generalEvents.push_back(newTempo);
                //mpqnToBpm(newTempo->microsecondsPerQuarternote);
                //std::cout << "The number of microseconds per quarter note are: " << std::dec << microsecondsPerQuarternote << "\n";

            }
            else if (metaMessageStatus == midi_meta_messageStatus::timeSignature) {

                midi_timeSignature* newTimeSignature = new midi_timeSignature;
                newTimeSignature->startTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                newTimeSignature->endTime = quantize(cummulativeDeltaTime, ticksPerQuarterNote);
                readData<uint8_t>(newTimeSignature->numerator, file_reader);
                readData<uint8_t>(newTimeSignature->denominator, file_reader);
                readData<uint8_t>(newTimeSignature->clocksPerMetronomeTick, file_reader);
                readData<uint8_t>(newTimeSignature->notesPerQuarterNote, file_reader);
                //generalEvents.push_back(newTimeSignature);
                timingEvents.push_back(newTimeSignature);


                //std::cout << "The numerator is : " << unsigned(numerator) << "\n";
                //std::cout << "The denominator is 2 to the power of : -" << unsigned(denominator) << "\n";


            }
            /*
            else if (metaMessageStatus == midi_meta_messageStatus::instrumentName) {
                std::string text;
                uint8_t temp = 0;
                for (int i = 0; i < eventLength; i++) {
                    readData<uint8_t>(temp, file_reader);
                    text += char(temp);
                }
                std::cout << text;
            }
            */
            
            else {
                readStream_throwaway(file_reader, eventLength);
            }


        }




        //USELESS EVENTS
        else if (status > 0xbf && status < 0xe0) { //1 byte
            readStream_throwaway(file_reader, 1);
        }
        else if (status >= 0xf0 && status < 0xff) { //unlimited bytes
            uint32_t eventLength = readVariableLengthQuantity(file_reader);
            readStream_throwaway(file_reader, eventLength);

        }
        else if (status < 0xc0 && status > 0x9f || tempStatus == 0xe0) {//2 bytes
            readStream_throwaway(file_reader, 2);
        }



        else {

            std::cerr << "Error - unidentifiable status byte " << "\n";
        }

        //std::cout << "Delta time: " << std::dec << deltaTime << "\n";
        //std::cout << "Cummulative delta time: " << cummulativeDeltaTime << "\n" ;
        //std::cout << "tpqn, denominator, dt: " << std::dec << microsecondsPerQuarternote << " " << unsigned(denominator) << " " << deltaTime << "\n";
        //dtToBb(ticksPerQuarterNote, denominator, deltaTime); //look at this - its broken
        //std::cout << "------------------------------------------" << "\n";
    }

    //std::cout << "End of track" << "\n";
    //std::cin.get();

    //Must sort before splitting up the notes
    std::sort(finishedNotes.begin(), finishedNotes.end(),
        [](midi_note* a, midi_note* b)->bool {
            return a->startTime < b->startTime;
        });



    //SPLIT UP NOTES
    //For every note, we need to make sure it fits in a bar. This means we have to split up the notes which don't fit a bar, and give them a tied mark
    //The tied mark can be either up down, or both
    //Assuming that we know the amount of ticks in a bar:
    //Assuming that we have a vector of midi_timeSignature events which indicates when the time Sig changes 
    int counter = 0;
    uint32_t barlineTicks = getBarlineTicks(timingEvents[counter]->numerator, timingEvents[counter]->denominator, ticksPerQuarterNote);// time of 2/4
    uint32_t subtractedNoteDuration = 0;
    for (auto &i : finishedNotes) {
        if (counter < timingEvents.size() - 1) { //still have timing events left to resolve
            if (i->startTime >= timingEvents[counter+1]->startTime) { //the timing event occurs earlier than the note
                barlineTicks = getBarlineTicks(timingEvents[counter+1]->numerator, timingEvents[counter+1]->denominator, ticksPerQuarterNote); //change barlineTicks
                counter++;
                splitNotes(i, finishedNotesSplit, subtractedNoteDuration, barlineTicks);
            }
            else {
                if (i->endTime < timingEvents[counter + 1]->startTime) { //the timing event occurs between the start and end of the note
                    midi_note* newNote = new midi_note; //Fill in note before the timing change
                    newNote->startTime = i->startTime;
                    newNote->endTime = timingEvents[counter + 1]->startTime;
                    newNote->channel = i->channel;
                    newNote->note = i->note;
                    newNote->velocity = i->velocity;

                    splitNotes(newNote, finishedNotesSplit, subtractedNoteDuration, barlineTicks, 0,1,1);
                    barlineTicks = getBarlineTicks(timingEvents[counter + 1]->numerator, timingEvents[counter + 1]->denominator, ticksPerQuarterNote); //change barlineTicks
                    
                    newNote = new midi_note; 
                    newNote->channel = i->channel;
                    newNote->note = i->note;
                    newNote->velocity = i->velocity;
                    newNote->startTime = i->startTime;//fill in note after timing change
                    newNote->endTime = timingEvents[counter + 1]->startTime;
                    splitNotes(newNote, finishedNotesSplit, subtractedNoteDuration, barlineTicks, 1,1,2);
                    counter++;
                }
                else {
                    splitNotes(i, finishedNotesSplit, subtractedNoteDuration, barlineTicks);
                }
            }
        }
        else {
            splitNotes(i, finishedNotesSplit, subtractedNoteDuration, barlineTicks);
        }
    }



    //ADD BARLINES
    //The barlineTicks may change with time -> we'll have to find all the different time signature change events.
    //We also have to split up the notes so they all fit within the bars, and no barlines are in the middle of a note;
    uint64_t cummulativeBarlineTicks = 0;
    bool endOfTimeSignatures = false;
    uint64_t limitTime = 0;
    counter = 0;
    uint32_t barNumber = 0;

    while (!endOfTimeSignatures) {
        if (counter < timingEvents.size() - 1) {
            limitTime = timingEvents[counter+1]->startTime;
        }
        else {
            limitTime = cummulativeDeltaTime;
            endOfTimeSignatures = true;
        }

        barlineTicks = getBarlineTicks(timingEvents[counter]->numerator, timingEvents[counter]->denominator, ticksPerQuarterNote);
        counter++;

        while (cummulativeBarlineTicks < limitTime) {
            midi_barline* newBarline = new midi_barline;
            newBarline->startTime = cummulativeBarlineTicks;
            newBarline->endTime = cummulativeBarlineTicks;
            newBarline->barline = true;
            newBarline->barNumber = barNumber;
            barNumber++;
            finishedNotesSplit.push_back(newBarline);
            cummulativeBarlineTicks += barlineTicks;
        }

    }



    //Sort all notes that have now been split up
    std::sort(finishedNotesSplit.begin(), finishedNotesSplit.end(),
        [](midi_event* a, midi_event* b)->bool {
            if (a->startTime == b->startTime && a->barline == true) {
                return true;
            }
            else if (a->startTime == b->startTime && b->barline == true) {
                return false;
            }
            else {
                return a->startTime < b->startTime;
            }
        });


   
    //ADD RESTS
    for (int i = 0; i < int(finishedNotesSplit.size()) - 1; i++) {
        if (long(finishedNotesSplit[i + 1]->startTime) - long(finishedNotesSplit[i]->endTime) > 0) {//If there is a rest between two consecutive notes
            //Create new rest (with type midi_note and note value of 0), populate values
            midi_rest* newRest = new midi_rest;
            newRest->startTime = finishedNotesSplit[i]->endTime;
            newRest->endTime = finishedNotesSplit[i + 1]->startTime;
            

            //Add the notes and rests to the complete notes and rests vector
            generalEvents.push_back(newRest);
        }
    }

    //finishedNotesSplit.erase(finishedNotesSplit.begin()); //destroy first empty event
    for (auto& i : finishedNotesSplit) {
        generalEvents.push_back(i);
    }

    //SORT EVENTS AND NOTES BY START TIME
    std::sort(generalEvents.begin(), generalEvents.end(),
        [](midi_event* a, midi_event* b)->bool {
            if (a->startTime == b->startTime && a->barline == true) {
                return true;
            }
            else if (a->startTime == b->startTime && b->barline == true) {
                return false;
            }
            else {
                return a->startTime < b->startTime;
            }

        });//Barlines take higher precedence than any other event


    //destroy the first barline
    //generalEvents.erase(generalEvents.begin());
    //std::cout << "Error here" << "\n";


    //PRINT TIMING EVENTS APPEARING ON TRACK
    for (auto& i : timingEvents) {
        outputEvents(ticksPerQuarterNote, i);
    }


    //PRINT OTHER EVENTS APPEARING ON TRACK
    for (auto& i : generalEvents) {
        outputEvents(ticksPerQuarterNote, i);
    }


    //FREE MEMORY
    for (auto& i : unfinishedNotes) {
        delete i;
    }
    for (auto& i : finishedNotes) {
        delete i;
    }
    for (auto& i : generalEvents) {
        delete i;
    }

}



void readStream(std::ifstream& file_reader, uint16_t ticksPerQuarterNote, std::vector<midi_timeSignature*>& timingEvents) { //reads 1 track worth of data from the file 

    uint32_t midi_track_signature = 0;
    uint32_t midi_track_length = 0;
    bool endOfTrack = false;


    readData<uint32_t>(midi_track_signature, file_reader);
    readData<uint32_t>(midi_track_length, file_reader);
    readStream_Error(midi_track_signature, midi_track_length, file_reader, endOfTrack);

    readStream_readEvent(file_reader, endOfTrack, ticksPerQuarterNote, timingEvents);
}



void readTracks(std::ifstream& file_reader, midi_header_info& midi_header) {

    uint16_t fileFormat = midi_header.fileFormat;
    uint16_t trackNumber = midi_header.trackNumber;
    uint16_t division = midi_header.division;

    //Tempo and time signature events only occur in the first MIDI track.


    //So we need to be able to give each call to the readStream function an argument which includes all the time sig changes inside a vector of type midi_timeSignature*
    //This means we need to construct a vector of this type, and push_back a midi_timeSignature of 4/4 at dt 0
    std::vector<midi_timeSignature*> timeSignatureMap;
    midi_timeSignature* newTimeSignature = new midi_timeSignature;
    newTimeSignature->denominator = 2;
    newTimeSignature->numerator = 4;
    newTimeSignature->startTime = 0;
    newTimeSignature->endTime = 0;
    newTimeSignature->clocksPerMetronomeTick = 0;
    newTimeSignature->notesPerQuarterNote = 0;
    timeSignatureMap.push_back(newTimeSignature);

    if (fileFormat == 0) {
        readStream(file_reader, division, timeSignatureMap);
    }
    
    else if (fileFormat == 1) {
        //Time signature map with 1st track,
        std::cout << "Reading tempo map track" << "\n";
        readStream(file_reader, division, timeSignatureMap);

        //Read stream for other tracks, using the time signature map to make barlines + split up notes
        for (int i = 1; i <= trackNumber - 1; i++) {
            std::cout << "\n\n\n\n\nReading track " << i+1 << "\n";
            readStream(file_reader, division, timeSignatureMap);
        }

    }

    else {
        std::cerr << "Error - unparseable file format" << "\n";
        endProgram();
    }
}

/*
void testVLQ() {
    std::ifstream file_reader;
    std::ofstream file_writer;
    std::string file_name = "testFile.mid";


    file_reader.open(file_name, std::ios::in | std::ios::binary);


    uint32_t normalValue = readVariableLengthQuantity(file_reader);

    std::cout << "Normal representation: " << std::hex << normalValue << "\n";
    file_reader.close();
    endProgram();
}
*/

void outputProgramInfo() {
    std::string explanation = R"(## INTRODUCTION AND GENERAL INFORMATION ##
This program reads a MIDI file and outputs the information needed to write the contents of that MIDI file on a stave.
When prompted, paste the file path into the program and press enter.
Then, the program will start to output the contents of the file.

The contents of the file will be outputted in tracks, one track at a time. 
Each track contains one section of the piece, e.g. piano part, violin 1 part, drumkit bass drum part etc.
A track is usually one instrument but one instrument can have multiple tracks (e.g. piano left hand, piano right hand).

Each track contains multiple events which create the piece. An event is an action which can be drawn on a stave (e.g. note, key signature, barline).
To get the next event in the piece, press the enter key.

In general, rhythms are written in american-english notation written in number form (e.g. eighth note = 1/8).
Rhythmic events which are a combination of two other rhythmic events are written with a + (e.g. dotted quarter note = 1/4+1/8)
The + can be written on a stave as a tie, or can be combined to form one note which is the correct dotted equivalent.



## FILE PATH ##
For those of you who don't know how to copy a file path, read these instructions:

1. Go to the folder in your files where you have the MIDI file you want to input.
2. Click once on the MIDI file
3. Look at the banner on the top of your file menu. There should be a button which says "Copy Path"
4. Click on the "Copy Path" button
5. Use the ctrl+v shortcut to paste in the file path that you have copied
6. Press enter to run the program.

These instructions should work on most Windows devices...



## NOTATION AIDS ##
For people not used to the american-english notation for describing note lengths such as whole note, half note, and quarter note, here is a table which may help you:

1 (whole note) = semibreve
1/2 (half note) = minim
1/4 (quarter note) = crotchet
1/8 (eighth note) = quaver
and so on...


In general, note events are written in scientific notation (e.g. c4, e5)
The user is responsible for being able to convert this notation onto a stave. This allows notes to be written on both treble and bass staves.

For people who don't know where scientific notation is placed on a stave, here is a rough guide.
Treble clef:                                                                                         
                                                -o-          A5  
                                             o               G5
-----------------------------------------o--------           F5
                                     o                       E5
---------------------------------o----------------           D5
                             o                               C5
-------------------------o------------------------           B4
                     o                                       A4
-----------------o--------------------------------           G4
             o                                               F4
---------o----------------------------------------           E4                                                                                     
     o                                                       D4
-o-                                                          C4



Bass Clef:
                                                -o-          C4
                                             o               B3
-----------------------------------------o--------           A3
                                     o                       G3
---------------------------------o----------------           F3
                             o                               E3
-------------------------o------------------------           D3
                     o                                       C2
-----------------o--------------------------------           B2
             o                                               A2
---------o----------------------------------------           G2                                                                                    
     o                                                       F2
-o-                                                          E2

and so on...



## BOUNDS OF IMPLEMENTATION ##
This program will display:
    - Notes
    - Rests
    - Barlines
    - Time signature changes
    - Tempo changes
    - Key signature changes

This program will not display:
    - Text such as instrument names or lyrics
    - Chords or multiple notes at once within one note event (when multiple notes are played at the same time, the start time of the notes will be the same)
    - Information on where events occur relative to the beginning of the bar (duration is relative from the beginning of the piece)

This program does not support:
    - Mac or Linux OS
    - MIDI files which are type 2
    - MIDI files which use the SMPTE format to calculate the amount of time in a delta time tick



## NOTES ##
Note: Some information such as notes being tied will be in the wrong place if there are multiple notes occuring at the same time. 
The user's discretion and common sense should be used here to fill in the gaps made by the program.

If there are any problems with the file, the program will automatically stop and an error message will be outputted. 
To try again, re-run the program. Enjoy!



)";
    std::cout << explanation;
}

std::string getUserFile() {
    std::string userInput;
    std::string finalFileName;
    std::cout << "Please enter your file's file path\n";
    std::getline(std::cin, userInput);
    for (auto i:  userInput) {

        if (i == '\\') {
            finalFileName += "\\";
        }
        else if(i != '\"'){
            finalFileName += i;
        }
    }
    return finalFileName;
}


int main()
{

    outputProgramInfo();

    std::ifstream file_reader;
    file_reader.open(getUserFile(), std::ios::in | std::ios::binary);
    if (file_reader.is_open()) {
        std::cout << "\nCommencing reading the file\n\n\n\n\n";
        midi_header_info midi_header; // this is where all MIDI header data is stored
        readHeader(file_reader, midi_header);



        readTracks(file_reader, midi_header);
        file_reader.close();

    }
    else {
        file_reader.close();
        std::cerr << "Error - could not open file" << "\n";
        endProgram();
    }
}