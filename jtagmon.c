#include <string.h>
#include <stdio.h>
#include "jtagmon.h"

enum tap_states {
	TEST_LOGIC_RESET,
	RUN_TEST_IDLE,
	SELECT_DR,
	CAPTURE_DR,
	SHIFT_DR,
	EXIT1_DR,
	PAUSE_DR,
	EXIT2_DR,
	UPDATE_DR,
	SELECT_IR,
	CAPTURE_IR,
	SHIFT_IR,
	EXIT1_IR,
	PAUSE_IR,
	EXIT2_IR,
	UPDATE_IR
};

void jtagmon(unsigned char tck, unsigned char tms, unsigned char tdi) {
	static unsigned char last_tck = 1;
	static char tdi_written = 0;
	static int state = TEST_LOGIC_RESET;
	static char state_text[32] = "Test Logic Reset";
	char last_state_text[32];
	int last_state = state;

	strcpy(last_state_text, state_text);

	if (!last_tck && tck) {
		switch(state) {
			case TEST_LOGIC_RESET:
				if (tms) {
					state = TEST_LOGIC_RESET;
				} else {
					state = RUN_TEST_IDLE;
				}
				break;
			case RUN_TEST_IDLE:
				if (tms) {
					state = SELECT_DR;
				} else {
					state = RUN_TEST_IDLE;
				}
				break;
			case SELECT_DR:
				if (tms) {
					state = SELECT_IR;
				} else {
					state = CAPTURE_DR;
				}
				break;
			case CAPTURE_DR:
				if (tms) {
					state = EXIT1_DR;
				} else {
					state = SHIFT_DR;
				}
				break;
			case SHIFT_DR:
				if (tms) {
					state = EXIT1_DR;
				} else {
					state = SHIFT_DR;
				}
				break;
			case EXIT1_DR:
				if (tms) {
					state = UPDATE_DR;
				} else {
					state = PAUSE_DR;
				}
				break;
			case PAUSE_DR:
				if (tms) {
					state = EXIT2_DR;
				} else {
					state = PAUSE_DR;
				}
				break;
			case EXIT2_DR:
				if (tms) {
					state = UPDATE_DR;
				} else {
					state = SHIFT_DR;
				}
				break;
			case UPDATE_DR:
				if (tms) {
					state = SELECT_DR;
				} else {
					state = RUN_TEST_IDLE;
				}
				break;
			case SELECT_IR:
				if (tms) {
					state = TEST_LOGIC_RESET;
				} else {
					state = CAPTURE_IR;
				}
				break;
			case CAPTURE_IR:
				if (tms) {
					state = EXIT1_IR;
				} else {
					state = SHIFT_IR;
				}
				break;
			case SHIFT_IR:
				if (tms) {
					state = EXIT1_IR;
				} else {
					state = SHIFT_IR;
				}
				break;
			case EXIT1_IR:
				if (tms) {
					state = UPDATE_IR;
				} else {
					state = PAUSE_IR;
				}
				break;
			case PAUSE_IR:
				if (tms) {
					state = EXIT2_IR;
				} else {
					state = PAUSE_IR;
				}
				break;
			case EXIT2_IR:
				if (tms) {
					state = UPDATE_IR;
				} else {
					state = SHIFT_IR;
				}
				break;
			case UPDATE_IR:
				if (tms) {
					state = SELECT_DR;
				} else {
					state = RUN_TEST_IDLE;
				}
				break;
		}

		switch(state) {
			case TEST_LOGIC_RESET:
				strcpy(state_text, "Test Logic Reset");
				break;
			case RUN_TEST_IDLE:
				strcpy(state_text, "Run-Test / Idle");
				break;
			case SELECT_DR:
				strcpy(state_text, "Select-DR");
				break;
			case CAPTURE_DR:
				strcpy(state_text, "Capture-DR");
				break;
			case SHIFT_DR:
				strcpy(state_text, "Shift-DR");
				break;
			case EXIT1_DR:
				strcpy(state_text, "Exit1-DR");
				break;
			case PAUSE_DR:
				strcpy(state_text, "Pause-DR");
				break;
			case EXIT2_DR:
				strcpy(state_text, "Exit2-DR");
				break;
			case UPDATE_DR:
				strcpy(state_text, "Update-DR");
				break;
			case SELECT_IR:
				strcpy(state_text, "Select-IR");
				break;
			case CAPTURE_IR:
				strcpy(state_text, "Capture-IR");
				break;
			case SHIFT_IR:
				strcpy(state_text, "Shift-IR");
				break;
			case EXIT1_IR:
				strcpy(state_text, "Exit1-IR");
				break;
			case PAUSE_IR:
				strcpy(state_text, "Pause-IR");
				break;
			case EXIT2_IR:
				strcpy(state_text, "Exit2-IR");
				break;
			case UPDATE_IR:
				strcpy(state_text, "Update-IR");
				break;
		}

		if (last_state != state) {
			if (tdi_written)
				fprintf(stderr,"\n");

			fprintf(stderr,"TAP state transition from %s to %s\n", last_state_text, state_text);
			tdi_written = 0;
		} else {
			fprintf(stderr,"%d",(tdi ? 1 : 0));
			tdi_written = 1;
		}
	}

	last_tck = tck;
}
