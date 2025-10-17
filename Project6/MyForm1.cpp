#include "MyForm1.h"
#include <windows.h>
#include <sstream>
#include <string>
#include "Main.h"

namespace Project6 {
    System::Void MyForm1::MyForm1_Load(System::Object^ sender, System::EventArgs^ e) {
        String^ text1;
        switch (PARITY) {
        case NOPARITY:
            text1 = "NOPARITY";
            break;
        case ODDPARITY:
            text1 = "ODDPARITY";
            break;
        case EVENPARITY:
            text1 = "EVENPARITY";
            break;
        default:
            text1 = "Unknown";
            break;
        }

        textBox4->Text = text1;

        String^ text2 = portX_1.ToString();
        textBox5->Text = text2;

        String^ text3 = portX_2.ToString();
        textBox6->Text = text3;

        String^ text4 = portY_1.ToString();
        textBox7->Text = text4;

        String^ text5 = portY_2.ToString();
        textBox8->Text = text5;

        listBox1->Items->Clear();
        listBox2->Items->Clear();
        listBox1->DrawMode = System::Windows::Forms::DrawMode::OwnerDrawFixed;
        listBox2->DrawMode = System::Windows::Forms::DrawMode::OwnerDrawFixed;

        // ��� g_originalPackets (����� ListBox)
        for (std::vector<BYTE> arr : g_originalPackets) {
            std::stringstream ss;
            for (BYTE el : arr) {
                ss << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << static_cast<int>(el) << " ";
            }
            String^ line = gcnew String(ss.str().c_str());
            listBox1->Items->Add(line);
        }

        // ��� g_stuffedPackets (������ ListBox)
        for (std::vector<BYTE> arr : g_stuffedPackets) {
            std::stringstream ss;
            for (BYTE el : arr) {
                ss << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << static_cast<int>(el) << " ";
            }
            String^ line = gcnew String(ss.str().c_str());
            listBox2->Items->Add(line);
        }

        // ������������� �� ������� ���������
        listBox1->DrawItem += gcnew System::Windows::Forms::DrawItemEventHandler(this, &MyForm1::listBox1_DrawItem);
        listBox2->DrawItem += gcnew System::Windows::Forms::DrawItemEventHandler(this, &MyForm1::listBox2_DrawItem);
    }

    // ���������� ��������� ��� listBox1
// ���������� ��������� ��� listBox1 (������������ ������)
    System::Void MyForm1::listBox1_DrawItem(System::Object^ sender, System::Windows::Forms::DrawItemEventArgs^ e) {
        if (e->Index < 0) return;

        e->DrawBackground();

        System::Windows::Forms::ListBox^ lb = (System::Windows::Forms::ListBox^)sender;
        String^ text = lb->Items[e->Index]->ToString();

        // ��������� ������ �� ��������� �����
        array<wchar_t>^ separators = { ' ' };
        array<String^>^ bytes = text->Split(separators, System::StringSplitOptions::RemoveEmptyEntries);

        float x = e->Bounds.X;
        float y = e->Bounds.Y;

        System::Drawing::Font^ font = e->Font;
        System::Drawing::Brush^ defaultBrush = System::Drawing::Brushes::Black;
        System::Drawing::Brush^ plusBrush = System::Drawing::Brushes::Green;  // ���� ��� �����
        System::Drawing::Brush^ minusBrush = System::Drawing::Brushes::Gray;  // ���� ��� ������

        // ��������� ����� ����� ������ ������
        for (int i = 0; i < bytes->Length; i++) {
            if (bytes[i]->Length == 0) continue;

            // ����������, ��� �� ����� ���� ������ JAM
            bool hadJamBefore = false;
            if (e->Index < g_jamPositions.size() && i < g_jamPositions[e->Index].size()) {
                hadJamBefore = g_jamPositions[e->Index][i];
            }

            // ������ ���� (+ ��� -) ����� ������
            String^ sign = hadJamBefore ? "+ " : "- ";
            System::Drawing::Brush^ signBrush = hadJamBefore ? plusBrush : minusBrush;

            System::Drawing::SizeF signSize = e->Graphics->MeasureString(sign, font);
            e->Graphics->DrawString(sign, font, signBrush, x, y);
            x += signSize.Width;

            // ������ ��� ����
            String^ byteText = bytes[i] + " ";
            e->Graphics->DrawString(byteText, font, defaultBrush, x, y);

            System::Drawing::SizeF byteSize = e->Graphics->MeasureString(byteText, font);
            x += byteSize.Width;
        }

        e->DrawFocusRectangle();
    }

    // ���������� ��������� ��� listBox2 (�������� ������)
    System::Void MyForm1::listBox2_DrawItem(System::Object^ sender, System::Windows::Forms::DrawItemEventArgs^ e) {
        if (e->Index < 0) return;

        e->DrawBackground();

        System::Windows::Forms::ListBox^ lb = (System::Windows::Forms::ListBox^)sender;
        String^ text = lb->Items[e->Index]->ToString();

        // ��������� ������ �� ��������� �����
        array<wchar_t>^ separators = { ' ' };
        array<String^>^ bytes = text->Split(separators, System::StringSplitOptions::RemoveEmptyEntries);

        float x = e->Bounds.X;
        float y = e->Bounds.Y;

        System::Drawing::Font^ font = e->Font;
        System::Drawing::Brush^ defaultBrush = System::Drawing::Brushes::Black;
        System::Drawing::Brush^ plusBrush = System::Drawing::Brushes::Green;  // ���� ��� �����
        System::Drawing::Brush^ minusBrush = System::Drawing::Brushes::Gray;  // ���� ��� ������

        // ��������� ����� ����� ������ ������
        for (int i = 0; i < bytes->Length; i++) {
            if (bytes[i]->Length == 0) continue;

            // ����������, ��� �� ����� ���� ������ JAM
            bool hadJamBefore = false;
            if (e->Index < g_jamPositions.size() && i < g_jamPositions[e->Index].size()) {
                hadJamBefore = g_jamPositions[e->Index][i];
            }

            // ������ ���� (+ ��� -) ����� ������
            String^ sign = hadJamBefore ? "+ " : "- ";
            System::Drawing::Brush^ signBrush = hadJamBefore ? plusBrush : minusBrush;

            System::Drawing::SizeF signSize = e->Graphics->MeasureString(sign, font);
            e->Graphics->DrawString(sign, font, signBrush, x, y);
            x += signSize.Width;

            // ������ ��� ����
            String^ byteText = bytes[i] + " ";
            e->Graphics->DrawString(byteText, font, defaultBrush, x, y);

            System::Drawing::SizeF byteSize = e->Graphics->MeasureString(byteText, font);
            x += byteSize.Width;
        }

        e->DrawFocusRectangle();
    }
}