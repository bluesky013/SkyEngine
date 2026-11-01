#include "IOSNativeTextInput.h"
#include "platform/NativeTextInputState.h"

#include <core/util/String.h>

#include <algorithm>
#include <cerrno>
#include <utility>

#import <UIKit/UIKit.h>

namespace {
    NSString *NativeString(const std::string &text)
    {
        return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
    }

    std::string EngineString(NSString *text)
    {
        std::u16string units(text.length, u'\0');
        for (NSUInteger index = 0; index < text.length; ++index) {
            units[index] = static_cast<char16_t>([text characterAtIndex:index]);
        }
        return sky::Utf16ToUtf8(units);
    }

    NSString *LimitText(NSString *text, uint32_t maximum)
    {
        if (maximum == 0) {
            return text;
        }
        NSUInteger offset = 0;
        uint32_t   count  = 0;
        while (offset < text.length && count < maximum) {
            const unichar high = [text characterAtIndex:offset++];
            if (high >= 0xD800 && high <= 0xDBFF && offset < text.length) {
                const unichar low = [text characterAtIndex:offset];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    ++offset;
                }
            }
            ++count;
        }
        return offset == text.length ? text : [text substringToIndex:offset];
    }

    BOOL AcceptChange(id<UITextInput> input, NSString *text, NSRange range, NSString *replacement, uint32_t maximum)
    {
        if (range.location > text.length || range.length > text.length - range.location) {
            return NO;
        }
        if (input.markedTextRange != nil || maximum == 0) {
            return YES;
        }
        NSString *next = [text stringByReplacingCharactersInRange:range withString:replacement];
        return LimitText(next, maximum).length == next.length;
    }
} // namespace

@interface                                            SkyIOSNativeTextInputHandle : NSObject
@property (nonatomic, assign) uint64_t                token;
@property (nonatomic, strong) UINavigationController *navigation;
@end

@implementation SkyIOSNativeTextInputHandle
@end

@interface SkyIOSNativeTextInputController : UIViewController <UITextFieldDelegate, UITextViewDelegate, UIAdaptivePresentationControllerDelegate>
@property (nonatomic, assign) uint64_t                   token;
@property (nonatomic, assign) uint32_t                   maximum;
@property (nonatomic, assign) BOOL                       multiline;
@property (nonatomic, assign) BOOL                       password;
@property (nonatomic, copy) NSString                    *initialText;
@property (nonatomic, copy) NSString                    *placeholder;
@property (nonatomic, copy) NSString                    *committedText;
@property (nonatomic, strong) UITextField               *field;
@property (nonatomic, strong) UITextView                *editor;
@property (nonatomic, strong) UILabel                   *placeholderLabel;
@property (nonatomic, weak) SkyIOSNativeTextInputHandle *handle;
- (void)changed;
- (void)finish:(sky::NativeTextInputEventType)type;
- (void)cancel;
- (void)confirm;
@end

@implementation SkyIOSNativeTextInputController
- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor                       = UIColor.systemBackgroundColor;
    self.navigationItem.leftBarButtonItem           = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                                                                          target:self
                                                                                          action:@selector(cancel)];
    self.navigationItem.rightBarButtonItem          = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                                                           target:self
                                                                                           action:@selector(confirm)];
    UIStackView *stack                              = [[UIStackView alloc] init];
    stack.axis                                      = UILayoutConstraintAxisVertical;
    stack.spacing                                   = UIStackViewSpacingUseSystem;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:stack];
    UILayoutGuide *margins = self.view.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor], [stack.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor]
    ]];
    NSLayoutYAxisAnchor *bottom = self.view.safeAreaLayoutGuide.bottomAnchor;
    if (@available(iOS 15.0, *)) {
        bottom = self.view.keyboardLayoutGuide.topAnchor;
    } else {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(keyboardFrameChanged:)
                                                     name:UIKeyboardWillChangeFrameNotification
                                                   object:nil];
    }
    if (self.multiline) {
        self.placeholderLabel           = [[UILabel alloc] init];
        self.placeholderLabel.text      = self.placeholder;
        self.placeholderLabel.textColor = UIColor.placeholderTextColor;
        self.placeholderLabel.font      = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
        self.placeholderLabel.hidden    = self.initialText.length != 0;
        [stack addArrangedSubview:self.placeholderLabel];
        self.editor                                   = [[UITextView alloc] init];
        self.editor.text                              = self.initialText;
        self.editor.font                              = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
        self.editor.adjustsFontForContentSizeCategory = YES;
        self.editor.delegate                          = self;
        self.editor.accessibilityLabel                = self.placeholder;
        [stack addArrangedSubview:self.editor];
        [stack.bottomAnchor constraintEqualToAnchor:bottom].active = YES;
    } else {
        self.field                                   = [[UITextField alloc] init];
        self.field.text                              = self.initialText;
        self.field.placeholder                       = self.placeholder;
        self.field.borderStyle                       = UITextBorderStyleRoundedRect;
        self.field.font                              = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
        self.field.adjustsFontForContentSizeCategory = YES;
        self.field.secureTextEntry                   = self.password;
        if (self.password) {
            self.field.autocorrectionType     = UITextAutocorrectionTypeNo;
            self.field.autocapitalizationType = UITextAutocapitalizationTypeNone;
            self.field.textContentType        = UITextContentTypePassword;
        }
        self.field.returnKeyType = UIReturnKeyDone;
        self.field.delegate      = self;
        [self.field addTarget:self action:@selector(changed) forControlEvents:UIControlEventEditingChanged];
        [stack addArrangedSubview:self.field];
        [stack.bottomAnchor constraintLessThanOrEqualToAnchor:bottom].active = YES;
    }
}

- (void)keyboardFrameChanged:(NSNotification *)notification
{
    NSValue *value = notification.userInfo[UIKeyboardFrameEndUserInfoKey];
    if (value == nil || self.view.window == nil) {
        return;
    }
    const CGRect windowFrame = [self.view.window convertRect:value.CGRectValue fromCoordinateSpace:self.view.window.screen.coordinateSpace];
    const CGRect frame       = [self.view convertRect:windowFrame fromView:self.view.window];
    const CGRect overlap     = CGRectIntersection(self.view.bounds, frame);
    UIEdgeInsets insets      = UIEdgeInsetsZero;
    if (!CGRectIsNull(overlap) && CGRectGetMaxY(overlap) >= CGRectGetMaxY(self.view.bounds)) {
        insets.bottom = overlap.size.height;
    }
    self.additionalSafeAreaInsets = insets;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    self.navigationController.presentationController.delegate = self;
    if (sky::NativeTextInputState::Get()->ActiveToken() != self.token) {
        [self.navigationController dismissViewControllerAnimated:NO completion:nil];
        return;
    }
    UIResponder *input = self.editor;
    if (!self.multiline) {
        input = self.field;
    }
    if (![input becomeFirstResponder]) {
        SkyIOSNativeTextInputHandle *handle = self.handle;
        sky::NativeTextInputState::Get()->Post(self.token, {sky::NativeTextInputEventType::ERROR, EngineString(self.committedText), EIO});
        [self.navigationController dismissViewControllerAnimated:NO
                                                      completion:^{
                                                          handle.navigation = nil;
                                                          sky::NativeTextInputState::Get()->Dispatch();
                                                      }];
    }
}

- (void)changed
{
    __attribute__((objc_precise_lifetime)) SkyIOSNativeTextInputController *keepAlive = self;
    (void)keepAlive;
    if (sky::NativeTextInputState::Get()->ActiveToken() != self.token) {
        return;
    }
    id<UITextInput> input = self.editor;
    if (!self.multiline) {
        input = self.field;
    }
    NSString *text               = self.multiline ? self.editor.text : self.field.text;
    self.placeholderLabel.hidden = text.length != 0;
    // UIKit owns marked text; imposing the limit before composition ends would corrupt IME editing.
    if (input.markedTextRange != nil) {
        return;
    }
    NSString *limited = LimitText(text, self.maximum);
    if (limited.length != text.length) {
        limited = self.committedText;
        if (self.multiline) {
            const NSUInteger caret    = std::min(self.editor.selectedRange.location, limited.length);
            self.editor.text          = limited;
            self.editor.selectedRange = NSMakeRange(caret, 0);
        } else {
            UITextRange    *selection = self.field.selectedTextRange;
            const NSInteger caret     = selection != nil ? [self.field offsetFromPosition:self.field.beginningOfDocument toPosition:selection.start]
                                                         : static_cast<NSInteger>(limited.length);
            self.field.text           = limited;
            UITextPosition *position =
                [self.field positionFromPosition:self.field.beginningOfDocument
                                          offset:std::min(std::max<NSInteger>(caret, 0), static_cast<NSInteger>(limited.length))];
            if (position != nil) {
                self.field.selectedTextRange = [self.field textRangeFromPosition:position toPosition:position];
            }
        }
    }
    self.placeholderLabel.hidden = limited.length != 0;
    self.committedText           = limited;
    if (sky::NativeTextInputState::Get()->Post(self.token, {sky::NativeTextInputEventType::CHANGED, EngineString(limited), 0})) {
        sky::NativeTextInputState::Get()->Dispatch();
    }
}

- (void)textViewDidChange:(UITextView *)textView
{
    (void)textView;
    [self changed];
}

- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text
{
    return AcceptChange(textView, textView.text, range, text, self.maximum);
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)text
{
    return AcceptChange(textField, textField.text, range, text, self.maximum);
}

- (void)textViewDidChangeSelection:(UITextView *)textView
{
    (void)textView;
    [self changed];
}

- (void)textFieldDidChangeSelection:(UITextField *)textField
{
    (void)textField;
    [self changed];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    (void)textField;
    [self confirm];
    return NO;
}

- (void)finish:(sky::NativeTextInputEventType)type
{
    UINavigationController      *navigation = self.navigationController;
    SkyIOSNativeTextInputHandle *handle     = self.handle;
    if (!sky::NativeTextInputState::Get()->Post(self.token, {type, EngineString(self.committedText), 0})) {
        return;
    }
    [self.view endEditing:YES];
    [navigation dismissViewControllerAnimated:YES
                                   completion:^{
                                       handle.navigation = nil;
                                       sky::NativeTextInputState::Get()->Dispatch();
                                   }];
}

- (void)cancel
{
    [self finish:sky::NativeTextInputEventType::CANCELLED];
}

- (void)confirm
{
    SkyIOSNativeTextInputController *controller = self;
    [controller.editor unmarkText];
    [controller.field unmarkText];
    [controller changed];
    [controller finish:sky::NativeTextInputEventType::CONFIRMED];
}

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    (void)presentationController;
    if (sky::NativeTextInputState::Get()->Post(self.token, {sky::NativeTextInputEventType::CANCELLED, EngineString(self.committedText), 0})) {
        self.handle.navigation = nil;
        sky::NativeTextInputState::Get()->Dispatch();
    }
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    UINavigationController *navigation = self.navigationController;
    if ((self.isBeingDismissed || navigation.isBeingDismissed || navigation.presentingViewController == nil) &&
        sky::NativeTextInputState::Get()->Post(self.token, {sky::NativeTextInputEventType::CANCELLED, EngineString(self.committedText), 0})) {
        self.handle.navigation = nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            sky::NativeTextInputState::Get()->Dispatch();
        });
    }
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}
@end

namespace sky::ios {
    bool ShowNativeTextInput(void *sceneHandle, const NativeTextInputOptions &options, NativeTextInputCallback callback, void *&dialog)
    {
        if (![NSThread isMainThread] || sceneHandle == nullptr || NativeTextInputState::Get()->ActiveToken() != 0 ||
            (options.multiline && options.password)) {
            return false;
        }
        UIWindowScene    *scene     = (__bridge UIWindowScene *)sceneHandle;
        UIViewController *presenter = nil;
        for (UIWindow *window in scene.windows) {
            if (window.isKeyWindow && !window.hidden) {
                presenter = window.rootViewController;
                break;
            }
        }
        NSString *text        = NativeString(options.text);
        NSString *placeholder = NativeString(options.placeholder);
        NSString *title       = NativeString(options.title);
        if (presenter == nil || presenter.view.window == nil || presenter.presentedViewController != nil || presenter.isBeingDismissed ||
            text == nil || placeholder == nil || title == nil || LimitText(text, options.maxLength).length != text.length) {
            return false;
        }
        HideNativeTextInput(dialog);
        const uint64_t token = NativeTextInputState::Get()->Begin(options, std::move(callback));
        if (token == 0) {
            return false;
        }
        SkyIOSNativeTextInputController *editor = [[SkyIOSNativeTextInputController alloc] init];
        if (editor == nil) {
            NativeTextInputState::Get()->Post(token, {NativeTextInputEventType::ERROR, options.text, ENOMEM});
            NativeTextInputState::Get()->Dispatch();
            return true;
        }
        editor.token                            = token;
        editor.maximum                          = options.maxLength;
        editor.multiline                        = options.multiline;
        editor.password                         = options.password;
        editor.initialText                      = text;
        editor.committedText                    = text;
        editor.placeholder                      = placeholder;
        editor.title                            = title;
        UINavigationController      *navigation = [[UINavigationController alloc] initWithRootViewController:editor];
        SkyIOSNativeTextInputHandle *handle     = [[SkyIOSNativeTextInputHandle alloc] init];
        if (navigation == nil || handle == nil) {
            NativeTextInputState::Get()->Post(token, {NativeTextInputEventType::ERROR, options.text, ENOMEM});
            NativeTextInputState::Get()->Dispatch();
            return true;
        }
        navigation.modalPresentationStyle = UIModalPresentationFormSheet;
        handle.token                      = token;
        handle.navigation                 = navigation;
        editor.handle                     = handle;
        dialog                            = (__bridge_retained void *)handle;
        [presenter presentViewController:navigation animated:YES completion:nil];
        if (navigation.presentingViewController == nil) {
            NativeTextInputState::Get()->Post(token, {NativeTextInputEventType::ERROR, options.text, EIO});
            HideNativeTextInput(dialog);
        }
        return true;
    }

    void HideNativeTextInput(void *&dialog)
    {
        SkyIOSNativeTextInputHandle *handle = (__bridge_transfer SkyIOSNativeTextInputHandle *)std::exchange(dialog, nullptr);
        const auto                   close  = ^{
            if (handle == nil) {
                NativeTextInputState::Get()->Cancel();
            } else {
                UINavigationController          *navigation = handle.navigation;
                SkyIOSNativeTextInputController *editor = static_cast<SkyIOSNativeTextInputController *>(navigation.viewControllers.firstObject);
                NativeTextInputState::Get()->Post(handle.token, {NativeTextInputEventType::CANCELLED, EngineString(editor.committedText), 0});
                [navigation.view endEditing:YES];
                [navigation dismissViewControllerAnimated:NO completion:nil];
                handle.navigation = nil;
            }
            NativeTextInputState::Get()->Dispatch();
        };
        if ([NSThread isMainThread]) {
            close();
        } else {
            dispatch_async(dispatch_get_main_queue(), close);
        }
    }
} // namespace sky::ios
