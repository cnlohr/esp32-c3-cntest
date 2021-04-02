//Copyright (C) 2016 <>< Charles Lohr, see LICENSE file for more info.
//
//This particular file may be licensed under the MIT/x11, New BSD or ColorChord Licenses.

var bIsInWait = false;
var excan;
var exctx;
var SetColors = [];
var buttons = [];

function PullI()
{
	if( !bIsInWait )
	{
		$('#InfoBtn').val('Getting data...');
		QueueOperation( "I" + SetColors[0] + " " + SetColors[1] + " " + SetColors[2] + " " + SetColors[3] + " " + SetColors[4] + " " + SetColors[5], clbInfoBtn ); // Send info request to ESP
	}
	bIsInWait = true;
	setTimeout( PullI, 1 );
}


function TEvent( x, y )
{
	x |= 0;
	y |= 0;

	var i;
	for( i = 0; i < buttons.length; i++ )
	{
		var b = buttons[i];
		if( b.x <= x && b.x + b.w >= x && b.y < y && b.y + b.h >= y )
		{
			if( b.drag )
			{
				b.drag( x - b.x, y - b.y, b );
			}
		}
	}

}

function canvasmousemove( e )
{
	var x = e.offsetX;
	var y = e.offsetY;
	
	if( e.buttons != 0 )
	{
		TEvent( x, y );
	}
}


// Get the position of a touch relative to the canvas
function getTouchPos(canvasDom, touchEvent) {
  var rect = canvasDom.getBoundingClientRect();
  return {
    x: touchEvent.touches[0].clientX - rect.left,
    y: touchEvent.touches[0].clientY - rect.top
  };
}

function CLAMPB(x)
{
	if( x < 0 ) return 0;
	if( x > 255 ) return 255;
	return x;
}

function initExample() {
var menItm = `
	<tr><td width=1><input type=submit onclick="ShowHideEvent( 'Example' );" value="Example"></td><td>
	<div id=Example class="collapsible">
	<p>I'm an example feature found in ./web/page/feature.example.js.</p>
	<input type=button id=InfoBtn value="Display Info"><br>
	<p id=InfoDspl>&nbsp;</p>
		<canvas id="exCanvas" width="800" height="500" style="border:1px solid #000000;" onmousemove="canvasmousemove">
		</canvas> 
	</div>
	</td></tr>
`;
	setTimeout( PullI, 1 );
	$('#MainMenu > tbody:last').after( menItm );

	excan = document.getElementById("exCanvas");
	exctx = excan.getContext("2d");
	excan.addEventListener( 'mousemove', canvasmousemove );
	excan.addEventListener( 'mousedown', canvasmousemove );

	excan.addEventListener("touchstart", function (e) { $('#InfoDspl').html( "X " + JSON.stringify( e.touches ) ); var p = getTouchPos( excan, e ); TEvent( p.x, p.y ); } );
	excan.addEventListener("touchmove", function (e) { $('#InfoDspl').html( "B " + JSON.stringify( e.touches ) );  var p = getTouchPos( excan, e ); TEvent( p.x, p.y ); } );
//	excan.addEventListener("touchend", function (e) { $('#InfoDspl').html( "E " + e.changedTouches[0] ); TEvent( e.changedTouches[0], e.changedTouches[0].y ); } );

/*
		 //   mousePos = getTouchPos(excan, e);
	  var touch = e.touches[0];
	  var mouseEvent = new MouseEvent("mousedown", {
		buttons: 1,
		clientX: touch.clientX,
		clientY: touch.clientY
	  });
	  excan.dispatchEvent(mouseEvent);
	}, false);
	excan.addEventListener("touchend", function (e) {
	  var mouseEvent = new MouseEvent("mouseup", {});
	  excan.dispatchEvent(mouseEvent);
	}, false);
	excan.addEventListener("touchmove", function (e) {
	  var touch = e.touches[0];
	  var mouseEvent = new MouseEvent("mousemove", {
		buttons: 1,
		clientX: touch.clientX,
		clientY: touch.clientY
	  });
	  excan.dispatchEvent(mouseEvent);
	}, false);
*/

	buttons = [];
	SetColors = [100, 100, 100, 100, 100, 100];
	buttons.push( { x : 540, y : 20, w : 265, h : 50, fillStyle : "red", text : "R1 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[0] = x; b.text = "R1 " + x; } } );
	buttons.push( { x : 540, y : 80, w : 265, h : 50, fillStyle : "green" , text : "G1 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[1] = x; b.text = "G1 " + x; } } );
	buttons.push( { x : 540, y : 140, w : 265, h : 50, fillStyle : "blue", text : "B1 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[2] = x; b.text = "B1 " + x; } } );
	buttons.push( { x : 540, y : 210, w : 265, h : 50, fillStyle : "red", text : "R2 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[3] = x; b.text = "R2 " + x; } } );
	buttons.push( { x : 540, y : 270, w : 265, h : 50, fillStyle : "green" , text : "G2 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[4] = x; b.text = "G2 " + x; } } );
	buttons.push( { x : 540, y : 330, w : 265, h : 50, fillStyle : "blue", text : "B2 100", drag : function( x, y, b ) { x=CLAMPB(x-5); SetColors[5] = x; b.text = "B2 " + x; } } );

	$('#InfoBtn').click( function(e) {
		$('#InfoBtn').val('Getting data...');
		QueueOperation( "I", clbInfoBtn ); // Send info request to ESP
	});
}

window.addEventListener("load", initExample, false);


function drawBar( x, y, x2, h )
{
	exctx.fillStyle = "blue";
	exctx.beginPath();
	exctx.fillRect( x, y, x2, -h);
	exctx.stroke();
	exctx.strokeStyle = "4px black";
	exctx.beginPath();
	exctx.strokeRect( x, y, x2, -h);
	exctx.stroke();
}

// Handle request previously sent on button click
function clbInfoBtn(req,data) {
	$('#InfoBtn').val('Display Info');
	bIsInWait = false;
//	$('#InfoDspl').html('Info returned from esp:<br>'+ data);

	var w = excan.clientWidth;
	var h = excan.clientHeight;
	exctx.fillStyle = "white";
	exctx.fillRect(0, 0, w, h);

	exctx.font = "20px Arial";
	exctx.fillStyle = "black";
	exctx.fillText(data, 1, 20);


	var dataElems = data.split( " " );

	drawBar( 5, h-5, 95, dataElems[1] * 0.2 );
	drawBar( 105, h-5, 95, dataElems[2] * 0.4 );
	drawBar( 205, h-5, 95, dataElems[3] * 0.2 );
	drawBar( 305, h-5, 95, dataElems[4] * 0.2 );

	for( var i = 0; i < buttons.length; i++ )
	{
		var b = buttons[i];
		exctx.fillStyle = b.fillStyle;
		exctx.beginPath();
		exctx.fillRect( b.x, b.y, b.w, b.h);
		exctx.stroke();
		exctx.strokeStyle = "4px black";
		exctx.beginPath();
		exctx.strokeRect( b.x, b.y, b.w, b.h);
		exctx.stroke();
		exctx.font = "15px Arial";
		exctx.fillStyle = "black";
		exctx.fillText(b.text, b.x, b.y+b.h-5);

	}

}
