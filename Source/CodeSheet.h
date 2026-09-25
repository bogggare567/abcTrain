#pragma once

#include <juce_core/juce_core.h>
#include "LocalRoom.h"
#include "shared/ui/QrCode.h"

// The printed cards of a "list only" local seminar: one card per person
// with the name, the personal code in large digits, and a QR code that
// opens the room with the code already in it. A4, 2 x 5 cards, to be cut.
//
// An HTML page, not a PDF: every computer can print one from its browser,
// with the system's own print dialog (paper size, printer, "save as PDF"),
// and the app needs no PDF writer for it. It asks to print when it opens.
namespace CodeSheet
{
    struct Strings
    {
        juce::String title { "Codes for the seminar" };
        juce::String howTo { "Connect to the Wi-Fi of the room, scan the code - or open the address and type the code." };
        juce::String code { "Your code" };
        juce::String print { "Print" };
    };

    inline juce::String escape (const juce::String& s)
    {
        return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;");
    }

    // `address` is "192.168.0.14:8930".
    inline juce::String html (const juce::String& roomTitle, const juce::String& address,
                              const std::vector<LocalRoom::Invitee>& people, const Strings& s)
    {
        juce::String cards;

        for (const auto& p : people)
        {
            const auto url = "http://" + address + "/?c=" + p.code;
            cards << "<div class=\"card\"><div class=\"text\"><div class=\"room\">" << escape (roomTitle) << "</div>"
                  << "<div class=\"name\">" << escape (p.name.isNotEmpty() ? p.name : p.mail) << "</div>"
                  << "<div class=\"cap\">" << escape (s.code) << "</div><div class=\"code\">" << escape (p.code) << "</div>"
                  << "<div class=\"addr\">http://" << escape (address) << "</div></div>"
                  << "<div class=\"qr\">" << QrCode (url).toSvg (3) << "</div></div>\n";
        }

        return "<!doctype html><html><head><meta charset=\"utf-8\"><title>" + escape (s.title) + "</title><style>"
               "@page{size:A4;margin:10mm}"
               "body{font-family:-apple-system,'Segoe UI',Roboto,Arial,sans-serif;color:#111;margin:0}"
               "header{display:flex;justify-content:space-between;align-items:baseline;margin:0 0 6mm}"
               "h1{font-size:18pt;margin:0}p{margin:2mm 0 0;font-size:10pt;color:#444}"
               "button{font-size:12pt;padding:6px 14px}"
               ".grid{display:grid;grid-template-columns:1fr 1fr;gap:0}"
               ".card{display:flex;justify-content:space-between;align-items:center;height:52mm;padding:5mm;box-sizing:border-box;"
               "border:1px dashed #999;break-inside:avoid;page-break-inside:avoid}"
               ".room{font-size:9pt;color:#555;text-transform:uppercase;letter-spacing:.08em}"
               ".name{font-size:15pt;font-weight:600;margin:1.5mm 0 3mm}"
               ".cap{font-size:9pt;color:#555}.code{font-size:30pt;font-weight:700;letter-spacing:.18em;font-variant-numeric:tabular-nums}"
               ".addr{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:9pt;color:#333;margin-top:2mm}"
               ".qr svg{width:36mm;height:36mm;display:block}"
               "@media print{button{display:none}}"
               "</style></head><body><header><div><h1>" + escape (roomTitle) + "</h1><p>" + escape (s.howTo)
               + "</p></div><button onclick=\"print()\">" + escape (s.print) + "</button></header><div class=\"grid\">"
               + cards + "</div><script>setTimeout(function(){print()},400)</script></body></html>";
    }

    // Writes the sheet to a temporary file and opens it in the browser.
    inline bool open (const juce::String& roomTitle, const juce::String& address,
                      const std::vector<LocalRoom::Invitee>& people, const Strings& s)
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("abcTrain").getChildFile ("codes-" + juce::String (juce::Time::currentTimeMillis()) + ".html");
        file.getParentDirectory().createDirectory();

        if (! file.replaceWithText (html (roomTitle, address, people, s), false, false, "\n"))
            return false;

        return file.startAsProcess();
    }
}
