void sound_enable () {

	TCCR1B |= (1 << CS10);
}

void sound_mute () {
	
	TCCR1B ^= (1 << CS10);
}

void play_note (short length, short pitch, short pause) {

	sound_enable ();

	OCR2A = pitch;
	delay (length);

	sound_mute ();

	delay (pause);

}

void beep () {

	play_note (200, 32, 100);
	play_note (200, 32, 100);
	play_note (200, 32, 100);
}