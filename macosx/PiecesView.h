// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import <AppKit/AppKit.h>

@class Torrent;

@interface PiecesView : NSImageView

@property(nonatomic) Torrent* torrent;

- (void)clearView;
- (void)updateView;

@end
