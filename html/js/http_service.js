'use strict';
var $ = require('jquery');

module.exports = class CGIRequest {
	//1ショット cgi実行
	onshot(cmd) {
		//cgiのget request
		return $.ajax({
			type: 'GET',
			url: '/cgi-bin/' + cmd,
			timeout: 10000
		})
	}

	//loopcgiの実行
	loopcgi(cmd, interval, chunked_callback) {
		return $.ajax({
			type: 'GET',
			url: '/loopcgi',
			//Queryの指定
			data: {
				'cgi': cmd,
				'interval': interval,
			},
			xhrFields: {
				//response開始
				onloadstart: function() {
					var xhr = this;
					this.timer = setInterval(function() {
						chunked_callback(xhr.responseText);
					}, interval*1000);
            			},
				//終了時
				onloadend: function() {
					clearInterval(this.timer);
				}
        		},
			success: function() {
				// すぐにクリアしてしまうと最終的なレスポンスに対する処理ができないので
				// タイマーと同じ間隔を空けてクリアする必要があるらしいです。
				setTimeout('clearInterval(this.timer)', interval*1000);
			},
			error: function() {
				console.log("Stop request " + cmd);
			}
		})
	}
}
