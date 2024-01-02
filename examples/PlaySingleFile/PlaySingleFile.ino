/**
 * Plays a single audio file on an attached USB dive
 */

#include <GigaAudio.h>

GigaAudio audio("USB_VOL"); // replace with name of USB volume

void setup() {
    Serial.begin(9600);

    if (!audio.load("song.wav")) {  // replace with name of file to play
        if (audio.hasError()) Serial.println(audio.errorMessage());
        else Serial.println("Cannot load WAV file");
        return;
    }

    Serial.println("Starting . . .");
    audio.play();
}

void loop() {
    if (audio.isFinished()) {
        audio.play(); // restart the playback when it is complete
        Serial.println("Restarting . . .");
    }
}