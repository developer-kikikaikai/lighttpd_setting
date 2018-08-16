const CGIRequest = require('./http_service.js');
const View = require('./view.js');
var cgireq = new CGIRequest();
var view = new View();

var loop_jqxhr = null;

function loop_encoding_chunked (result) {
}

function loop_success (result) {
	document.getElementById('response_data').innerHTML = result
}

function loop_failure (result) {
	console.log("loop_failure " + result)
}

function loop_clear() {
	console.log("loop_clear")
	if (loop_jqxhr !== null) {
		loop_jqxhr.abort();
		loop_jqxhr = null
	}

	console.log(document.getElementById('response_data').innerHTML)
	document.getElementById('response_data').textContent = ""
}

function call_cmd(cmd, loopinterval) {
	//2重起動防止
	loop_clear()

	//コマンド実行
	console.log(cmd)
	console.log(loopinterval)
	loop_jqxhr = cgireq.loop(cmd, loopinterval);
	loop_jqxhr.then(loop_success, loop_failure);
	//loop_jqxhr.xhrFields = 
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
view.add_stop_event(loop_clear)
