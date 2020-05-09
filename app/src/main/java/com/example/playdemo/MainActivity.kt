package com.example.playdemo

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.os.Bundle
import android.os.Environment
import android.view.View
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import cn.com.bamboo.easy_common.help.Permission4MultipleHelp
import cn.com.bamboo.easy_file_manage.FileManageActivity
import cn.com.bamboo.easy_file_manage.FileManageActivity.Companion.RESULT_JSON
import cn.com.bamboo.easy_file_manage.util.FILE_REQUEST
import com.google.gson.Gson
import com.google.gson.JsonArray
import com.google.gson.reflect.TypeToken
import kotlinx.android.synthetic.main.activity_main.*
import org.jetbrains.anko.toast
import org.json.JSONArray


class MainActivity : AppCompatActivity() {

//   香港财经
    private val rtmp1 = "rtmp://58.200.131.2:1935/livetv/hunantv"
//    湖南卫视
    private val rtmp = "rtmp://58.200.131.2:1935/livetv/hunantv"

    private var _progress: Int = 0
    private var isTouch = false
    private var isSeek = false
    private var isPause = true

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    val play = Play(object : OnPlayListener {
        override fun message(message: String) {
            runOnUiThread { ToastUtil.showShort(this@MainActivity, message); }
        }

        override fun onProgress(progress: Int, duration: Int) {
            if (!isTouch) {
                runOnUiThread(Runnable {
                    text.text = formatTime(progress)
                    //如果是直播
                    if (duration != 0) {
                        if (isSeek) {
                            isSeek = false
                            return@Runnable
                        }
                        //更新进度 计算比例
                        seek_bar.setProgress(progress * 100 / duration)
                    }
                })
            }
        }
    })

    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        //权限申请
        Permission4MultipleHelp.request(this, arrayOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.INTERNET
        ), success = {
            edit_text.setText(rtmp)
            edit_loacl_text.setText(getInnerSDCardPicturesPath())
            play.setSurfaceView(surface_view)
            button1.setOnClickListener {
                seek_bar.visibility = View.VISIBLE
                button_pause.visibility = View.VISIBLE
                play.setDataSource(edit_loacl_text.text.toString())
                play.prepare()
            }
            button2.setOnClickListener { play.stop() }
            button3.setOnClickListener {
                if (this.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE)
                } else {
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT)
                }
            }
            button4.setOnClickListener {
                seek_bar.visibility = View.GONE
                button_pause.visibility = View.GONE
                play.setDataSource(edit_text.text.toString())
                play.prepare()
            }
            button5.setOnClickListener {
                Intent(this, FileManageActivity::class.java).apply {
                    putExtra(FileManageActivity.GET_PATHS, true)
                    putExtra(FileManageActivity.GET_PATH_TYPE, 1)
                    startActivityForResult(this, FILE_REQUEST)
                }
            }

            button_pause.setOnClickListener {
                if (play.getDuration() == 0) {
                    return@setOnClickListener
                }
                if (isPause) {
                    button_pause.setImageDrawable(resources.getDrawable(android.R.drawable.ic_media_pause))
                    isPause = false
                } else {
                    button_pause.setImageDrawable(resources.getDrawable(android.R.drawable.ic_media_play))
                    isPause = true
                }

                play.pause()
            }
            seek_bar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(p0: SeekBar?, p1: Int, p2: Boolean) {
                }

                override fun onStartTrackingTouch(p0: SeekBar?) {
                    isTouch = true
                }

                override fun onStopTrackingTouch(p0: SeekBar) {
                    isSeek = true
                    isTouch = false
                    _progress = play.getDuration() * seek_bar.getProgress() / 100
                    //进度调整
                    //进度调整
                    play.seek(_progress)
                }
            })

            // Example of a call to a native method
            sample_text.text = stringFromJNI()
        }, fail = { toast("请开启读写权限") })

    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == FILE_REQUEST && resultCode == Activity.RESULT_OK && data != null) {
            val jsonArray = JSONArray(data.getStringExtra(RESULT_JSON))
            edit_loacl_text.setText(jsonArray[0].toString())
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        play.release()
    }

    override fun onResume() {
        super.onResume()
//        play.prepare()
    }

    override fun onStop() {
        super.onStop()
        play.stop()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            window.setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
            )
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
            layouot.layoutParams = lp

        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
            val lp = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 600)
            layouot.layoutParams = lp
        }
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String


    fun getInnerSDCardPicturesPath(): String {
        return Environment.getExternalStorageDirectory().getPath() + "/Pictures/test.mp4"
    }

    fun getInnerSDCard(): String {
        return Environment.getExternalStorageDirectory().getPath() + "/test.mp4"
    }

    fun formatTime(time: Int): String {
        if (time <= 0) {
            return "00:00"
        }
        var m = time / 60
        var s = time - (m * 60)

        return "$m:$s"
    }
}
