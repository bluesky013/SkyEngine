package {{ANDROID_NAMESPACE}};

import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.NoCopySpan;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.SpanWatcher;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import androidx.annotation.Keep;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.androidgamesdk.GameActivity;

public class SkyEngineActivity extends GameActivity {

    private Runnable unregisterBackCallback;
    private AlertDialog textInputDialog;
    private EditText textInputView;
    private long textInputToken;
    private String committedText = "";
    private boolean textInputResumed;

    private static final int TEXT_CHANGED = 0;
    private static final int TEXT_CONFIRMED = 1;
    private static final int TEXT_CANCELLED = 2;
    private static final int TEXT_ERROR = 3;

    @Keep
    private static native boolean nativeTextInputEvent(long token, int type, String text, int error);
    @Keep
    private static native boolean nativeTextInputActive(long token);

    static {
        System.loadLibrary("{{LAUNCHER_LIB_NAME}}");
    }

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode
                    = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        }
        // From API 30 onwards, this is the recommended way to hide the system UI, rather than
        // using View.setSystemUiVisibility.
        View decorView = getWindow().getDecorView();
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(),
                decorView);
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.hide(WindowInsetsCompat.Type.displayCutout());
        controller.setSystemBarsBehavior(
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        hideSystemUI();
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            unregisterBackCallback = Api33BackCallback.register(this);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        textInputResumed = true;
    }

    @Override
    protected void onPause() {
        textInputResumed = false;
        finishNativeTextInput(textInputToken, TEXT_CANCELLED, 0);
        super.onPause();
    }

    @Keep
    public void showNativeTextInput(long token, String text, String placeholder, String title,
            int maxLength, boolean multiline, boolean password) {
        runOnUiThread(() -> {
            if (!nativeTextInputActive(token)) {
                return;
            }
            if (!textInputResumed || isFinishing() || isDestroyed()
                    || getWindow() == null || getWindow().getDecorView().getWindowToken() == null) {
                postNativeTextInputEvent(token, TEXT_CANCELLED, text, 0);
                return;
            }
            if (textInputToken != 0 || (multiline && password) || maxLength < 0) {
                postNativeTextInputEvent(token, TEXT_ERROR, text, -2);
                return;
            }
            final EditText input = new EditText(this);
            input.setSingleLine(!multiline);
            input.setInputType(InputType.TYPE_CLASS_TEXT
                    | (multiline ? InputType.TYPE_TEXT_FLAG_MULTI_LINE : 0)
                    | (password ? InputType.TYPE_TEXT_VARIATION_PASSWORD : 0));
            if (multiline) {
                input.setMinLines(3);
                input.setMaxLines(6);
            }
            input.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                    | (multiline ? EditorInfo.IME_FLAG_NO_ENTER_ACTION : EditorInfo.IME_ACTION_DONE));
            if (maxLength != 0) {
                input.setFilters(new InputFilter[] {new ScalarLengthFilter(maxLength)});
            }
            input.setHint(placeholder);
            input.setText(text);
            input.setSelection(input.length());
            textInputToken = token;
            textInputView = input;
            committedText = input.getText().toString();
            final ScrollView content = new ScrollView(this);
            content.setFillViewport(true);
            content.addView(input, new ScrollView.LayoutParams(
                    ScrollView.LayoutParams.MATCH_PARENT, ScrollView.LayoutParams.WRAP_CONTENT));
            final LinearLayout layout = new LinearLayout(this);
            layout.setOrientation(LinearLayout.VERTICAL);
            if (!title.isEmpty()) {
                final TextView heading = new TextView(this);
                heading.setTextAppearance(android.R.style.TextAppearance_Material_Title);
                heading.setAccessibilityHeading(true);
                heading.setSingleLine(true);
                heading.setEllipsize(TextUtils.TruncateAt.END);
                heading.setText(title);
                layout.addView(heading);
            }
            final LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.addView(content, new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f));
            final Button cancel = new Button(this);
            cancel.setId(android.R.id.button2);
            cancel.setText(android.R.string.cancel);
            cancel.setOnClickListener(ignored -> finishNativeTextInput(token, TEXT_CANCELLED, 0));
            row.addView(cancel);
            final Button confirm = new Button(this);
            confirm.setId(android.R.id.button1);
            confirm.setText(android.R.string.ok);
            confirm.setOnClickListener(ignored -> finishNativeTextInput(token, TEXT_CONFIRMED, 0));
            row.addView(confirm);
            layout.addView(row);
            final AlertDialog dialog = new AlertDialog.Builder(this)
                    .setView(layout)
                    .create();
            textInputDialog = dialog;
            dialog.setOnCancelListener(ignored -> finishNativeTextInput(token, TEXT_CANCELLED, 0));
            dialog.setOnDismissListener(ignored -> finishNativeTextInput(token, TEXT_CANCELLED, 0));
            input.setOnEditorActionListener((view, action, event) -> {
                if (!multiline && action == EditorInfo.IME_ACTION_DONE) {
                    finishNativeTextInput(token, TEXT_CONFIRMED, 0);
                    return true;
                }
                return false;
            });
            final CommittedTextWatcher watcher = new CommittedTextWatcher(token, input);
            input.addTextChangedListener(watcher);
            input.getText().setSpan(watcher, 0, input.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            if (dialog.getWindow() == null) {
                finishNativeTextInput(token, TEXT_ERROR, -3);
                return;
            }
            configureTextInputWindow(dialog.getWindow());
            dialog.show();
            if (!input.requestFocus()) {
                finishNativeTextInput(token, TEXT_ERROR, -5);
                return;
            }
            new WindowInsetsControllerCompat(dialog.getWindow(), input).show(WindowInsetsCompat.Type.ime());
        });
    }

    @Keep
    public void hideNativeTextInput(long token) {
        runOnUiThread(() -> finishNativeTextInput(token, TEXT_CANCELLED, 0));
    }

    // Floating dialogs must resize so their actions remain above the IME.
    @SuppressWarnings("deprecation")
    private static void configureTextInputWindow(Window window) {
        window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }

    private void postNativeTextInputEvent(long token, int type, String text, int error) {
        if (!nativeTextInputEvent(token, type, text, error)) {
            finishNativeTextInput(token, TEXT_ERROR, -1);
        }
    }

    private void finishNativeTextInput(long token, int type, int error) {
        if (token == 0 || textInputToken != token) {
            return;
        }
        final AlertDialog dialog = textInputDialog;
        String text = committedText;
        if (type == TEXT_CONFIRMED && textInputView != null) {
            textInputView.clearComposingText();
            text = textInputView.getText().toString();
        }
        textInputToken = 0;
        textInputDialog = null;
        textInputView = null;
        postNativeTextInputEvent(token, type, text, error);
        if (dialog != null) {
            dialog.dismiss();
        }
        if (!isFinishing() && !isDestroyed()) {
            hideSystemUI();
        }
    }

    // Composing can finish through span removal without another text edit.
    private final class CommittedTextWatcher implements TextWatcher, SpanWatcher, NoCopySpan {
        private final long token;
        private final EditText input;
        private boolean queued;

        CommittedTextWatcher(long token, EditText input) {
            this.token = token;
            this.input = input;
        }

        private void schedule() {
            if (queued || textInputToken != token) {
                return;
            }
            queued = true;
            if (!input.post(() -> {
                queued = false;
                if (textInputToken != token) {
                    return;
                }
                final Editable text = input.getText();
                if (BaseInputConnection.getComposingSpanStart(text) >= 0
                        || BaseInputConnection.getComposingSpanEnd(text) >= 0) {
                    return;
                }
                final String value = text.toString();
                if (!value.equals(committedText)) {
                    committedText = value;
                    postNativeTextInputEvent(token, TEXT_CHANGED, value, 0);
                }
            })) {
                queued = false;
                finishNativeTextInput(token, TEXT_ERROR, -4);
            }
        }

        @Override
        public void beforeTextChanged(CharSequence text, int start, int count, int after) {}
        @Override
        public void onTextChanged(CharSequence text, int start, int before, int count) {}
        @Override
        public void afterTextChanged(Editable text) { schedule(); }
        @Override
        public void onSpanAdded(Spannable text, Object span, int start, int end) {
            if (span != this) { schedule(); }
        }
        @Override
        public void onSpanRemoved(Spannable text, Object span, int start, int end) {
            if (span != this) { schedule(); }
        }
        @Override
        public void onSpanChanged(Spannable text, Object span, int oldStart, int oldEnd, int start, int end) {
            if (span != this) { schedule(); }
        }
    }

    private static final class ScalarLengthFilter implements InputFilter {
        private final int limit;

        ScalarLengthFilter(int limit) {
            this.limit = limit;
        }

        @Override
        public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
            final int keep = limit - Character.codePointCount(dest, 0, dstart)
                    - Character.codePointCount(dest, dend, dest.length());
            if (keep <= 0) {
                return "";
            }
            if (Character.codePointCount(source, start, end) <= keep) {
                return null;
            }
            final int cutoff = Character.offsetByCodePoints(source, start, keep);
            final SpannableString result = new SpannableString(source.subSequence(start, cutoff));
            if (source instanceof Spanned) {
                TextUtils.copySpansFrom((Spanned) source, start, cutoff, Object.class, result, 0);
            }
            return result;
        }
    }

    // Route Back through the Activity policy before native game input consumes it.
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode != KeyEvent.KEYCODE_BACK) {
            return super.onKeyDown(keyCode, event);
        }
        if (event.getRepeatCount() == 0) {
            event.startTracking();
        }
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode != KeyEvent.KEYCODE_BACK) {
            return super.onKeyUp(keyCode, event);
        }
        if (event.isTracking() && !event.isCanceled()) {
            getOnBackPressedDispatcher().onBackPressed();
        }
        return true;
    }

    @Override
    protected void onDestroy() {
        textInputResumed = false;
        finishNativeTextInput(textInputToken, TEXT_CANCELLED, 0);
        if (unregisterBackCallback != null) {
            unregisterBackCallback.run();
            unregisterBackCallback = null;
        }
        super.onDestroy();
    }

    @TargetApi(Build.VERSION_CODES.TIRAMISU)
    private static final class Api33BackCallback {
        static Runnable register(SkyEngineActivity activity) {
            OnBackInvokedDispatcher dispatcher = activity.getOnBackInvokedDispatcher();
            OnBackInvokedCallback callback = () -> activity.getOnBackPressedDispatcher().onBackPressed();
            dispatcher.registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            return () -> dispatcher.unregisterOnBackInvokedCallback(callback);
        }
    }
}
