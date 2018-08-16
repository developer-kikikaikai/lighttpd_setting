'use strict';
var $ = require('jquery');

module.exports = class CommandListView {
	update_commandlists(cmd_list, cmd_callback) {
		var html;
		html = '<tr><th class="commandlists_th">cgi command</th></tr>'
		$('.commandlists').append(html);
		for(let i = 0; i < cmd_list.length; i++) {
			html = '<tr><td class="commandlists_td" id="'+cmd_list[i]+'">' + cmd_list[i] + '</td></tr>'
			$('.commandlists').append(html);
			//イベント設定
			$(document).on('click', '[id="'+cmd_list[i]+'"]', function(){
				console.log($(this).text())
				//インターバル取得
				
				var loopcount = document.getElementById('loopcount').value
				cmd_callback($(this).text(), loopcount)
				
			});
		}
	}

	add_stop_event(stop_callback) {
		$(document).on('click', '[id="loop_clear"]', stop_callback)
	}
}
