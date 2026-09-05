package org.pvz.recode;

import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;

/** 在 SDL 画面下方保留触屏工具栏；全部操作经 SDL 输入队列交给游戏线程。 */
public final class GameActivity extends SDLActivity {
    private Button rightButton;
    private LinearLayout toolbar;

    @Override protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null || mSurface == null) return;
        int height = Math.round(44 * getResources().getDisplayMetrics().density);
        RelativeLayout.LayoutParams surface = new RelativeLayout.LayoutParams(-1, -1);
        surface.bottomMargin = height;
        mSurface.setLayoutParams(surface);
        toolbar = new LinearLayout(this);
        toolbar.setGravity(Gravity.CENTER);
        toolbar.setBackgroundColor(0xff253329);
        RelativeLayout.LayoutParams bar = new RelativeLayout.LayoutParams(-1, height);
        bar.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        mLayout.addView(toolbar, bar);
        addButton("取消", KeyEvent.KEYCODE_F10);
        rightButton = addButton("右键点选", KeyEvent.KEYCODE_F9);
        addButton("暂停", KeyEvent.KEYCODE_ESCAPE);
    }

    /** 延后释放，让按下沿至少跨越一个逻辑步；按钮本身不向游戏发送左键。 */
    private Button addButton(String label, int key) {
        Button button = new Button(this);
        button.setText(label);
        button.setFocusable(false);
        button.setOnClickListener(view -> {
            if (key == KeyEvent.KEYCODE_F9) rightButton.setText("请点目标");
            if (key == KeyEvent.KEYCODE_F10) rightButton.setText("右键点选");
            onNativeKeyDown(key);
            button.postDelayed(() -> onNativeKeyUp(key), 100);
        });
        toolbar.addView(button, new LinearLayout.LayoutParams(0, -1, 1));
        return button;
    }

    @Override public boolean dispatchTouchEvent(MotionEvent event) {
        if (rightButton != null && toolbar != null
                && event.getActionMasked() == MotionEvent.ACTION_DOWN
                && event.getY() < toolbar.getTop()) rightButton.setText("右键点选");
        return super.dispatchTouchEvent(event);
    }
}
