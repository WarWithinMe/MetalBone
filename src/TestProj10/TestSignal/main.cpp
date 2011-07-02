#include "MBGlobal.h"
#include <stdio.h>
#include <string>

class slotClass
{
	public:
		void test() {
			fprintf(stdout, "%s\n", "Testing slot 'test'");
		}
		void test1(int c) {
			fprintf(stdout, "%s:%d\n", "Testing slot 'test1'",c);
		}
		void test2(int c,int b) {
			fprintf(stdout, "%s:%d,%d\n", "Testing slot 'test2'",c,b);
		}
		void test3(int c,int b,int c2) {
			fprintf(stdout, "%s:%d,%d,%d\n", "Testing slot 'test3'",c,b,c2);
		}
		void test4(int c,int b,int c2, int c3) {
			fprintf(stdout, "%s:%d,%d,%d,%d\n", "Testing slot 'test4'",c,b,c2,c3);
		}
		void test5(int c,int b,int c2, int c3,int c4) {
			fprintf(stdout, "%s:%d,%d,%d,%d,%d\n", "Testing slot 'test2'",c,b,c2,c3,c4);
		}
		void test6(int c,int b,int c2, int c3,int c4,int c5) {
			fprintf(stdout, "%s:%d,%d,%d,%d,%d,%d\n", "Testing slot 'test2'",c,b,c2,c3,c4,c5);
		}
		void test7(int c,int b,int c2, int c3,int c4,int c5,int c6) {
			fprintf(stdout, "%s:%d,%d,%d,%d,%d,%d,%d\n", "Testing slot 'test2'",c,b,c2,c3,c4,c5,c6);
		}
		void test8(int c,int b,int c2, int c3,int c4,int c5,int c6,int c7) {
			fprintf(stdout, "%s:%d,%d,%d,%d,%d,%d,%d,%d\n", "Testing slot 'test2'",c,b,c2,c3,c4,c5,c6,c7);
		}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd )
{
	// ========== Normal Emition ==========
	// Testing slot 'test'
	// Testing slot 'test1':1
	// Testing slot 'test2':1,2
	// Testing slot 'test3':1,2,3
	// Testing slot 'test4':1,2,3,4
	// Testing slot 'test2':1,2,3,4,5
	// Testing slot 'test2':1,2,3,4,5,6
	// Testing slot 'test2':1,2,3,4,5,6,7
	// Testing slot 'test2':1,2,3,4,5,6,7,8
	Signal0<> signal;
	slotClass slot;
	signal.Connect(&slot,&slotClass::test);
	signal.Emit();

	Signal1<int> signal1;
	signal1.Connect(&slot,&slotClass::test1);
	signal1.Emit(1);

	Signal2<int,int> signal2;
	signal2.Connect(&slot,&slotClass::test2);
	signal2.Emit(1,2);

	Signal3<int,int,int> signal3;
	signal3.Connect(&slot,&slotClass::test3);
	signal3.Emit(1,2,3);

	Signal4<int,int,int,int> signal4;
	signal4.Connect(&slot,&slotClass::test4);
	signal4.Emit(1,2,3,4);

	Signal5<int,int,int,int,int> signal5;
	signal5.Connect(&slot,&slotClass::test5);
	signal5.Emit(1,2,3,4,5);

	Signal6<int,int,int,int,int,int> signal6;
	signal6.Connect(&slot,&slotClass::test6);
	signal6.Emit(1,2,3,4,5,6);

	Signal7<int,int,int,int,int,int,int> signal7;
	signal7.Connect(&slot,&slotClass::test7);
	signal7.Emit(1,2,3,4,5,6,7);

	Signal8<int,int,int,int,int,int,int,int> signal8;
	signal8.Connect(&slot,&slotClass::test8);
	signal8.Emit(1,2,3,4,5,6,7,8);

	// ========== Slot Copying ==========
	// Testing slot 'test'
	// Testing slot 'test'
	// Testing slot 'test1':1
	// Testing slot 'test1':1
	// Testing slot 'test2':1,2
	// Testing slot 'test2':1,2
	// Testing slot 'test3':1,2,3
	// Testing slot 'test3':1,2,3
	// Testing slot 'test4':1,2,3,4
	// Testing slot 'test4':1,2,3,4
	// Testing slot 'test2':1,2,3,4,5
	// Testing slot 'test2':1,2,3,4,5
	// Testing slot 'test2':1,2,3,4,5,6
	// Testing slot 'test2':1,2,3,4,5,6
	// Testing slot 'test2':1,2,3,4,5,6,7
	// Testing slot 'test2':1,2,3,4,5,6,7
	// Testing slot 'test2':1,2,3,4,5,6,7,8
	// Testing slot 'test2':1,2,3,4,5,6,7,8
	slotClass slot2(slot);
	signal();
	signal1(1);
	signal2(1,2);
	signal3(1,2,3);
	signal4(1,2,3,4);
	signal5(1,2,3,4,5);
	signal6(1,2,3,4,5,6);
	signal7(1,2,3,4,5,6,7);
	signal8(1,2,3,4,5,6,7,8);

	// ========== Signal::disConnect ==========
	// 不会输出任何结果
	Signal1<int> newSignal1;
	slotClass newSlot1;
	newSignal1.Connect(&newSlot1,&slotClass::test1);
	newSignal1.Connect(&newSlot1,&slotClass::test1);
	newSignal1.Connect(&newSlot1,&slotClass::test1);
	newSignal1.Connect(&newSlot1,&slotClass::test1);
	newSignal1.Disconnect(&newSlot1,&slotClass::test1);
	newSignal1(5);
}
