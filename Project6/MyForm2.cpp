#include "MyForm2.h"
#include "Main.h"
#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>


namespace Project6 {
	System::Void MyForm2::comboBox1_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e) {
		String^ selectedText = comboBox1->SelectedItem->ToString();

		int number;
		Int32::TryParse(selectedText, number);

		portX_1 = number;
		portX_2 = number + 1;
		portY_1 = number + 2;
		portY_2 = number + 3;
	}

	System::Void MyForm2::comboBox2_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e) {
		String^ selectedText = comboBox2->SelectedItem->ToString();

		if (selectedText == "NOPARITY") {
			PARITY = NOPARITY;
		}
		else if (selectedText == "ODDPARITY") {
			PARITY = ODDPARITY;
		}
		else if (selectedText == "EVENPARITY") {
			PARITY = EVENPARITY;
		}
	}
}

