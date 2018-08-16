'use strict';
var $ = require('jquery');

module.exports = class CGIRequest {
	onshot(cmd) {
		//cgiのget request
		return $.ajax({
			type: 'GET',
			url: '/cgi-bin/' + cmd,
			timeout: 10000
		})
	}

	loopcgi(cmd, interval) {
		return $.ajax({
			type: 'GET',
			url: '/cgi-bin/' + cmd,
			//Queryの指定
			data: {
				'interval': interval,
				'cgi': cmd,
			},
			timeout: 10000
		})
	}

	loop(cmd, interval) {
		if ( interval === 0 ) {
			return this.onshot(cmd)
		} else {
			return this.loopcgi(cmd, interval)
		}
	}
}
