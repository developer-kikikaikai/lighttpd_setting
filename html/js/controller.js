const CGIRequest = require('./http_service.js');
const View = require('./view.js');
var cgireq = new CGIRequest();
var view = new View();

var loop_jqxhr = null;

function loop_encoding_chunked (result) {
	document.getElementById('response_data').innerHTML = result
}

function loop_success (result) {
	document.getElementById('response_data').innerHTML = result
	loop_jqxhr = null
}

function loop_failure (result) {
	loop_clear()
}

function loop_stop() {
	if (loop_jqxhr != null) {
		loop_jqxhr.abort();
		loop_jqxhr = null
	}
}

function loop_clear() {
	loop_stop()
	document.getElementById('response_data').textContent = ""
}

function call_cmd(cmd, loopinterval) {
	//2重起動防止
	loop_clear()

	//コマンド実行
	if ( loopinterval === 0 ) {
		loop_jqxhr = cgireq.onshot(cmd);
		loop_jqxhr.then(loop_success, loop_failure)
	} else {
		//loop再生
		loop_jqxhr = cgireq.loopcgi(cmd, loopinterval, loop_encoding_chunked);
	}
}

function command_list_success(result) {
	//commandをリスト化して
	view.update_commandlists(result.split('\n'), call_cmd)
}

function command_list_failure(result) {
	console.log(result)
}

//コマンド一覧の追加
cgireq.onshot('command_list.py').then(command_list_success, command_list_failure);
//キャンセルイベントの追加
view.add_stop_event(loop_stop)
