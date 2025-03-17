# include "generalHelpers.h"

void searchAndReplace(String *htmlString, String findPattern, String replaceWith)
{
  int index = 0;
  while ((index = htmlString->indexOf(findPattern, index)) != -1)
  {
    htmlString->replace(findPattern, replaceWith);
    // Update index to search for next occurrence
    index += replaceWith.length();
  }
}


void printBinary(int v, int num_places)
{
	int mask = 0, n;

	for (n = 1; n <= num_places; n++) {
		mask = (mask << 1) | 0x0001;
	}
	v = v & mask;  // truncate v to specified number of places

	while (num_places) {

		if (v & (0x0001 << (num_places - 1))) {
			Serial.print("1");
		}
		else {
			Serial.print("0");
		}

		--num_places;
		if (((num_places % 4) == 0) && (num_places != 0)) {
			Serial.print("_");
		}
	}
	Serial.println("");
}
