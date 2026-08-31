/* NSAlert + NSPopUpButton preset picker (spartan by decision — the shell stays the pretty
   face). Quiet mode never reaches AppKit: the answer comes from quiet-answers.json.       */
#import <AppKit/AppKit.h>
#include "MincPicker.h"
#include "MincSettings.h"
#include "MincPresets.h"
#include "MincJson.h"

bool MincPickPreset(std::string *keyOut) {
    if (MincQuietMode()) {
        MincJsonPtr qa = MincJsonParseFile(MincSettingsDir() + "/quiet-answers.json");
        if (!qa) return false;
        std::string k = qa->str("preset");
        if (k.empty() || !MincPresetMeta(k).valid) return false;
        *keyOut = k;
        return true;
    }
    @autoreleasepool {
        MincJsonPtr j = MincJsonParseFile(MincPresetsFileUsed());
        if (!j) { MincPresetMeta("acescg"); j = MincJsonParseFile(MincPresetsFileUsed()); }
        MincJsonPtr live = j ? j->get("presets") : nullptr;
        if (!live || live->obj.empty()) return false;
        NSPopUpButton *pop = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 260, 26) pullsDown:NO];
        NSMutableArray *keys = [NSMutableArray array];
        for (auto &kv : live->obj) {
            std::string label = kv.second ? kv.second->str("label", kv.first.c_str()) : kv.first;
            [pop addItemWithTitle:[NSString stringWithUTF8String:label.c_str()]];
            [keys addObject:[NSString stringWithUTF8String:kv.first.c_str()]];
        }
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"minColor preset";
        alert.informativeText = @"Working-space preset for this project.";
        alert.accessoryView = pop;
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        if ([alert runModal] != NSAlertFirstButtonReturn) return false;
        NSInteger i = [pop indexOfSelectedItem];
        if (i < 0 || i >= (NSInteger)[keys count]) return false;
        *keyOut = [[keys objectAtIndex:i] UTF8String];
        return true;
    }
}
