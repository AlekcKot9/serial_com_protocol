#include "MyForm.h"
#include "Main.h"
#include <string>
#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>

namespace Project6 {
	System::Void MyForm::button1_Click(System::Object^ sender, System::EventArgs^ e)
	{
		MyForm1^ controlForm = gcnew MyForm1();
		controlForm->Show();
	}

	System::Void MyForm::button2_Click(System::Object^ sender, System::EventArgs^ e)
	{
		MyForm2^ controlForm = gcnew MyForm2();
		controlForm->Show();
	}

	System::Void MyForm::textBox1_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e) {
		if (e->KeyChar == (char)13)
		{
			textBox3->Text = String::Empty;
			String^ inputText = textBox1->Text;
			std::string input = msclr::interop::marshal_as<std::string>(inputText);

			std::string output = workFirst(input);
			String^ outputText = msclr::interop::marshal_as<String^>(output);

			textBox3->Text = outputText;
			textBox1->Text = String::Empty;
		}
	}

	System::Void MyForm::textBox2_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e) {
		if (e->KeyChar == (char)13)
		{
			textBox4->Text = String::Empty;
			String^ inputText = textBox2->Text;
			std::string input = msclr::interop::marshal_as<std::string>(inputText);

			std::string output = workSecond(input);
			String^ outputText = msclr::interop::marshal_as<String^>(output);

			textBox4->Text = outputText;
			textBox2->Text = String::Empty;
		}
	}
}