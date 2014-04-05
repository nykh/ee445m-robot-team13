#define FILTER_LENGTH 51
const long ScaleFactor = 16384;
const long H[51]={-11,10,9,-5,1,0,-19,6,48,-12,-92,
     17,155,-20,-243,22,370,-24,-559,24,881,-24,-1584,24,4932,
     8578,4932,24,-1584,-24,881,24,-559,-24,370,22,-243,-20,155,
     17,-92,-12,48,6,-19,0,1,-5,9,10,-11};


long x[2*FILTER_LENGTH]; // this MACQ needs twice the size of FILTER_LENGTH
unsigned char n = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]

// Filter_ClearMemo
// this is called when one stops using the filter
// such that when filter is being used again the filter memory will be fresh
void Filter_ClearMemo(void) {
  unsigned char i;
  for(i = 0; i < 2*FILTER_LENGTH; ++i) x[i] = 0;
  n = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]
}

// Filter
// Digital FIR filter, assuming fs=1 Hz
// Coefficients generated with FIRdesign64.xls
// 
// y[i]= (h[0]*x[i]+h[1]*x[i-1]+…+h[63]*x[i-63])/256;
long Filter(long data) {
  long y = 0;
  unsigned char i;
  
  if(++n == 2*FILTER_LENGTH) n = FILTER_LENGTH;
  x[n] = x[n-FILTER_LENGTH] = data;
  
  // Assuming there is no overflow
  for(i = 0; i < FILTER_LENGTH; ++i){
		y += H[i]*x[n-i];
	}
  y /= ScaleFactor;
  
  return y;
}
