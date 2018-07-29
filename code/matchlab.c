// Eric Waugh u0947296

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char* aMethod(char* input, int word)
{
	int len = strlen(input);
	int status = 0;
	int oCount = 0;
	int digCount = 0;
	char* result = "no";
	int complete = 0;
	char firstChar;
	if(word)
	{
		result = input;
		firstChar = input[0];
	}

	int i;
	for (i = 0; i < len; i++)
	{
		switch(status)
		{
			case 0 : // checks that it is either 'b' or ',' when it is ',' it moves on
				if(input[i] != 'b' && input[i] != ',')
				{
					i = len;
					break;
				}
				if(input[i] == ',')
					status = 1;
				if(word)
					result[i] = firstChar;
				break;

			case 1 : // checks that it is either 'o' or ':' when it is ':' there must be 1 to 4 'o's
				if(input[i] != 'o' && input[i] != ':')
				{
					i = len;
					break;
				}
				if(input[i] == 'o')
					oCount++;
				else if(input[i] == ':' && oCount >= 1 && oCount <= 4)
					status = 2;
				if(word)
					result[i] = firstChar;
				break;

			case 2 : // checks for 1 to 3 instances of '0'(48) to '9'(57)
				if(input[i] >= 48 && input[i] <= 57)
				{
					digCount++;
					complete = 1;
				}
				else
				{
					complete = 0;
					i = len;
					break;
				}
				if(digCount > 3)
				{
					complete = 0;
					i = len;
					break;
				}
				if(word)
					result[i] = firstChar;
				break;

			default :
				break;
		}
	}
	if(complete && !word) // when it is correct input and no -t flat then it converts "no" to "yes"
	{
		result = "yes";
	}
	if(!complete && word) // when word is true but not right input sets to null string
	{
		result = "";
	}
	return result;
}

char* XStoreHelper(char* X, char input)
{
	int xSize = strlen(X);
	char* newX = malloc(xSize + sizeof(char) + 1);
	memcpy(newX + 0, X + 0, xSize);
	char* newValue = &input;
	*newValue = input;
	memcpy(newX + xSize, newValue + 0, sizeof(char) + 1);
	return newX;
}

char* addCHelper(char* currentResult, int i, int* cCountP) //adds C following H
{
	char* newResult = malloc(strlen(currentResult) + sizeof(char) + 1); // allocate for current string plus new char plus 1 for terminator
	memcpy(newResult + 0, currentResult + 0, i + *cCountP + 1); // copies up to and including H
	newResult[i + 1 + *cCountP] = 'C'; // sets following H to C
	char* resultFollowingH = malloc(strlen(currentResult));
	memcpy(resultFollowingH + 0, currentResult + i + 1 + *cCountP, strlen(currentResult) + 1); // gets what follows H
	memcpy(newResult + strlen(newResult), resultFollowingH + 0, strlen(resultFollowingH) + 1); // combines them
	*cCountP =  *cCountP + 1;
	return newResult;
}

char* bMethod(char* input, int word)
{
	int len = strlen(input);
	int status = 0;
	char* result = "no";
	int complete = 0;
	int oddLetterCount = 0;
	int yCount = 0;
	int digCount = 0;
	char* X = "";
	int cCount;
	int* cCountP = &cCount;
	*cCountP = 0;
	int XSpot = 0;
	int XCount = 0;
	if(word)
		result = input;

	int i;
	for (i = 0; i < len; i++)
	{
		switch(status)
		{
			case 0 : // checks that it is either 'm' or '_' when it is '_' it moves on
				if(input[i] != 'm' && input[i] != '_')
				{
					i = len;
					break;
				}
				if(input[i] == '_')
					status = 1;
				break;
			case 1 : // checks for upercase letters being at an odd count and moves on when it finds a 'y'. upercase letters are 65 to 90
				if(input[i] != 'y' && input[i] < 65 && input[i] > 90)
				{
					i = len;
					break;
				}
				if(input[i] >= 65 && input[i] <= 90)
				{
					oddLetterCount++;
					X = XStoreHelper(X, input[i]);

					if(input[i] == 'H' && word)
						result = addCHelper(result, i, cCountP);
				}
				else
				{
					if(oddLetterCount & 1) // checks if its odd
					{
						status = 2;
						yCount++;
					}
					else
					{
						i = len;
						break;
					}
				}
				break;

			case 2 : // checks that it is either 'y' or '_' also when it is '_' it will move on if there are 2 to 5 'y's
				if(input[i] != 'y' && input[i] != '_')
				{
					i = len;
					break;
				}
				if(input[i] == 'y')
					yCount++;
				else if(input[i] == '_' && yCount >= 2 && yCount <= 5)
					status = 3;

				if(yCount > 5)
				{
					i = len;
					break;
				}
				break;

			case 3 : // checks for 1 to 3 instances of '0'(48) to '9'(57) drops through to start checking for 4 cases of X
				if(input[i] >= 48 && input[i] <= 57)
				{
					digCount++;
					break;
				}
				if(digCount > 3)
				{
					i = len;
					break;
				}
				if(input[i] >= 65 && input[i] <= 90 && digCount > 0)
				{
					status = 4;
				}

			case 4 :
				if(input[i] != X[XSpot])
				{
					i = len;
					break;
				}
				if(input[i] == 'H' && word)
						result = addCHelper(result, i, cCountP);

				XSpot++;
				if(XSpot > strlen(X) - 1)
				{
					XCount++;
					XSpot = 0;
				}
				if(XCount == 4 && i == len - 1) // to signify that we have counted 4 and are at end of input
				{
					complete = 1;
					break;
				}
				break;

			default :
				break;

		}
	}

	if(complete && !word) // when it is correct input and no -t flat then it converts "no" to "yes"
	{
		result = "yes";
	}
	if(!complete && word) // when word is true but not right input sets to null string
	{
		result = "";
	}
	return result;
}

char* removeHHelper(char* currentResult, int i, int* hCountP)
{
	char* newResult = malloc(strlen(currentResult)); // allocate for current string would be minus 1 char and plus 1 for terminator but those cancel out
	memcpy(newResult + 0, currentResult + 0, i - *hCountP); // copies up to and including H
	char* resultFollowingH = malloc(strlen(currentResult));
	memcpy(resultFollowingH + 0, currentResult + i + 1 - *hCountP, strlen(currentResult) + 1); // gets what follows H
	memcpy(newResult + strlen(newResult), resultFollowingH + 0, strlen(resultFollowingH) + 1); // combines them
	*hCountP =  *hCountP + 1;
	return newResult;
}

char* cMethod(char* input, int word)
{
	int len = strlen(input);
	int status = 0;
	char* result = "no";
	int complete = 0;
	int oddCCount = 0;
	int oddLetterCount = 0;
	int qCount = 0;
	int digCount = 0;
	char* X = "";
	int hCount;
	int* hCountP = &hCount;
	*hCountP = 0;
	int XSpot = 0;
	if(word)
		result = input;

	int i;
	for (i = 0; i < len; i++)
	{
		switch(status)
		{
			case 0 : // checks that it is either 'c' or '_' when it is '_' it moves on if cCount is odd
				if(input[i] != 'c' && input[i] != '_')
				{
					i = len;
					break;
				}
				if(input[i] == 'c')
					oddCCount++;
				else if(input[i] == '_' && oddCCount & 1)
					status = 1;
				break;

			case 1 : // checks for upercase letters being at an odd count and moves on when it finds a 'q'. upercase letters are 65 to 90
				if(input[i] != 'q' && input[i] < 65 && input[i] > 90)
				{
					i = len;
					break;
				}
				if(input[i] >= 65 && input[i] <= 90)
				{
					oddLetterCount++;
					X = XStoreHelper(X, input[i]);

					if(input[i] == 'H' && word)
						result = removeHHelper(result, i, hCountP);
				}
				else
				{
					if(oddLetterCount & 1) // checks if its odd
					{
						XSpot = strlen(X) - 1;
						status = 2;
						qCount++;
					}
					else
					{
						i = len;
						break;
					}
				}
				break;

			case 2 : // checks that it is either 'q' or '_' also when it is '_' it will move on if there are 2 to e 'q's
				if(input[i] != 'q' && input[i] != '_')
				{
					i = len;
					break;
				}
				if(input[i] == 'q')
					qCount++;
				else if(input[i] == '_' && qCount >= 2 && qCount <= 3)
					status = 3;

				if(qCount > 3)
				{
					i = len;
					break;
				}
				break;

			case 3 : // checks that X is in reverse order
				if(input[i] != X[XSpot])
				{
					i = len;
					break;
				}
				if(input[i] == 'H' && word)
					result = removeHHelper(result, i, hCountP);

				XSpot--;
				if(XSpot < 0)
					status = 4;
				break;

			case 4 : // checks for 1 to 3 instances of '0'(48) to '9'(57)
				if(input[i] >= 48 && input[i] <= 57)
				{
					digCount++;
					complete = 1;
				}
				else
				{
					complete = 0;
					i = len;
					break;
				}
				if(digCount > 3)
				{
					complete = 0;
					i = len;
					break;
				}
				break;

			default :
				break;
		}	
	}

	if(complete && !word) // when it is correct input and no -t flat then it converts "no" to "yes"
	{
		result = "yes";
	}
	if(!complete && word) // when word is true but not right input sets to null string
	{
		result = "";
	}
	return result;
}

int main(int argc, char* argv[])
{
	int word = 0; // word is a boolean
	int argStart = 1;
	if((argv[1][0] == '-' && argv[1][1] == 't') || (argc > 2 && argv[2][0] == '-' && argv[2][1] == 't'))
	{
		argStart++;
		word = 1;
	}
	if((argv[1][0] == '-' && argv[1][1] == 'b') || (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'b')) // for b case flag
	{
		argStart++;
		int i = 0;
		for (i = argStart; i < argc; i++)
		{
			char* result = bMethod(argv[i], word);
			if(result[0])
				printf("%s \n", result);
		}
	}
	else if((argv[1][0] == '-' && argv[1][1] == 'c') || (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'c')) // for c case flag
	{
		argStart++;
		int i = 0;
		for (i = argStart; i < argc; i++)
		{
			char* result = cMethod(argv[i], word);
			if(result[0])
				printf("%s \n", result);
		}
	}
	else if((argv[1][0] == '-' && argv[1][1] == 'a') || (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'a')) // for a case when a flag is passed
	{
		argStart++;
		int i = 0;
		for (i = argStart; i < argc; i++)
		{
			char* result = aMethod(argv[i], word);
			if(result[0])
				printf("%s \n", result);
		}
	}
	else // defualt to do a if no flag
	{
		int i = 0;
		for (i = argStart; i < argc; i++)
		{
			char* result = aMethod(argv[i], word);
			if(result[0])
				printf("%s \n", result);
		}
	}
	return 0;
}