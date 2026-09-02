/* Native badge menu (macOS) — blocking NSMenu popup at the mouse, returns picked index or -1.
   Called from PF_Event_DO_CLICK on the main thread. The nested runloop it spins is safe under
   the single-walker rule: the edit marker is only written AFTER the menu closes, so no
   menu-triggered walk can land mid-menu (v2 P2; the stash's MincMenuGuard stays a contingency).
   Compiled with ARC. */
#import <AppKit/AppKit.h>

@interface MincMenuTarget : NSObject
@property (nonatomic) int picked;
- (void)minc_pick:(id)sender;
@end
@implementation MincMenuTarget
- (void)minc_pick:(id)sender { self.picked = (int)[(NSMenuItem *)sender tag]; }
@end

extern "C" int MincShowMenu(const char *const *items, int n, int checkedIdx) {
    @autoreleasepool {
        MincMenuTarget *tgt = [MincMenuTarget new];
        tgt.picked = 0;
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@"minColor"];
        [menu setAutoenablesItems:NO];
        for (int i = 0; i < n; ++i) {
            NSString *title = [NSString stringWithUTF8String:items[i] ? items[i] : ""];
            NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:(title ?: @"?")
                                                        action:@selector(minc_pick:)
                                                 keyEquivalent:@""];
            it.target = tgt;
            it.tag = i + 1;
            it.enabled = YES;
            if (i == checkedIdx) it.state = NSControlStateValueOn;
            [menu addItem:it];
        }
        [menu popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];
        return tgt.picked - 1;                     /* 0 = dismissed -> -1 */
    }
}
