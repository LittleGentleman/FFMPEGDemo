package com.ffmpegtest.gmm.www;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    // Used to load the 'native-lib' library on application startup.
//    static {
//        System.loadLibrary("native-lib");
//    }
    EnjoyPlayer player;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
//        TextView tv = (TextView) findViewById(R.id.sample_text);
//        tv.setText(stringFromJNI());

        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(this);

        player = new EnjoyPlayer();
//        player.setDataSource("/sdcard/1589793996212.mp4");
//        player.setDataSource("/sdcard/output_compose_video.mp4");
//        player.setDataSource("/sdcard/录屏_20170531_143341.mp4");
        player.setDataSource("/sdcard/1566884435_mv2025_video0.mov");
//        player.setDataSource("https://mv2025-project-video.oss-cn-beijing.aliyuncs.com/test/video_2020062314470.mp4");
        player.setOnPrepareListener(new EnjoyPlayer.OnPrepareListener() {
            @Override
            public void onPrepare() {
                player.start();
            }
        });
        player.prepare();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Surface surface = holder.getSurface();
        player.setSurface(surface);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.stop();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
//    public native String stringFromJNI();
}



