function bbimg(o){
	var zoom=parseInt(o.style.zoom, 10)||100;zoom+=event.wheelDelta/12;if (zoom>0) o.style.zoom=zoom+'%';
	return false;
}

function openScript(url, width, height){
	var Win = window.open(url,"openScript",'width=' + width + ',height=' + height + ',resizable=1,scrollbars=yes,menubar=no,status=yes' );
}
function chkdiv(divid){
	var chkid=document.getElementById(divid);
	if(chkid != null){return true; }
	else {return false; }
}

function getpara(){
	var str,pos,parastr
	str = window.location.href;
	pos = str.indexOf("?");
	parastr = str.substring(pos+1);
	return parastr;
}

function oblog_ViewCode(rnum)
{
	var bodyTag="<html><head><style type=text/css>.quote{margin:5px 20px;border:1px solid #CCCCCC;padding:5px; background:#F3F3F3 }\nbody{boder:0px}.HtmlCode{margin:5px 20px;border:1px solid #CCCCCC;padding:5px;background:#FDFDDF;font-size:14px;font-family:Tahoma;font-style : oblique;line-height : normal ;font-weight:bold;}\nbody{boder:0px}</style></head><BODY bgcolor=\"#FFFFFF\" >";
	bodyTag+=document.getElementById('scode'+rnum).value
	bodyTag+="</body></html>"
	preWin=window.open('preview','','left=0,top=0,width=550,height=400,resizable=1,scrollbars=1, status=1, toolbar=1, menubar=0');
	preWin.document.open();
	preWin.document.write(bodyTag);
	preWin.document.close();
	preWin.document.title="查看代码内容";
	preWin.document.charset="UTF-8";
}

function dialog(blogurl){
	var titile = '';
	var width = 300;
	var height = 150;
	var src = "";
	var path = blogurl+"images/dialog/";
	//alert(path);
	var sFunc = '<input id="dialogOk" type="button" style="font-size:12px;width:71px;height:22px;line-height:24px;border-style:none;background:transparent url('+path+'button4.bmp);width: 71px;height: 22px;"  onmouseover=BtnOver(this,"'+path+'") onmouseout=BtnOut(this,"'+path+'") value="确 认" onclick="new dialog(\''+blogurl+'\').reset();" /> <input id="dialogCancel" type="button" style="font-size:12px;width:71px;height:22px;line-height:24px;border-style:none;background:transparent url('+path+'button4.bmp);width: 71px;height: 22px;" value="取 消" onclick="new dialog(\''+blogurl+'\').reset();" />';
	var sClose = '<input type="image" title="关闭当前窗口" id="dialogBoxClose" onclick="new dialog(\''+blogurl+'\').reset();" src="' + path + 'dialogClose0.gif" border="0" width="16" height="16" onmouseover="this.src=\'' + path + 'dialogCloseF.gif\';" onmouseout="this.src=\'' + path + 'dialogClose0.gif\';" align="absmiddle" />';
	var sBody = '\
		<table id="dialogBodyBox" style="border:1px solid #54B1F7;border-top:0;background:url('+path+'sBody_bg.png) repeat-x left bottom;" border="0" align="center" cellpadding="0" cellspacing="0" width="100%" height="100%" >\
			<tr height="10"><td colspan="4"></td></tr>\
			<tr>\
				<td width="10"></td>\
				<td width="80" align="center" valign="middle" id="ob_boxface"><img id="dialogBoxFace" src="' + path + '1.png"  valign="absmiddle" style="behavior:url('+path+'png.htc);" /></td>\
				<td id="dialogMsg" style="font-size:12px;color:#000;"></td>\
				<td width="10"></td>\
			</tr>\
			<tr height="10"><td colspan="4" align="center"></td></tr>\
			<tr><td id="dialogFunc" colspan="4" align="center">' + sFunc + '</td></tr>\
			<tr height="10"><td colspan="4" align="center"></td></tr>\
		</table>\
	';
	var sBox = '\
		<table id="dialogBox" width="' + width + '" border="0" cellpadding="0" cellspacing="0" style="border:1px solid #1F3F94;display:none;z-index:10;">\
			<tr height="18" bgcolor="#1577D2" >\
				<td style="border:1px solid #91DEFA;border-bottom:0;">\
					<table onselectstart="return false;" style="-moz-user-select:none;background: url('+path+'win_top.png) repeat-x;" width="100%" border="0" cellpadding="0" cellspacing="0">\
						<tr>\
							<td width="6" ></td>\
							<td id="dialogBoxTitle" onmousedown="new dialog().moveStart(event, \'dialogBox\')" style="color:#fff;cursor:move;font-size:12px;font-weight:bold;">提示信息</td>\
							<td id="dialogClose" width="27" align="right" valign="middle">\
								' + sClose + '\
							</td>\
							<td width="6"></td>\
						</tr>\
					</table>\
				</td>\
			</tr>\
			<tr id="dialogHeight" height="' + height + '">\
				<td id="dialogBody" style="background:#fff;color:#000;border:1px solid #91DEFA;border-top:0;">' + sBody + '</td>\
			</tr>\
		</table>\
		<div id="dialogBoxShadow" style="display:none;z-index:9;"></div>\
		<iframe id="dialogBoxDivShim" scrolling="no" frameborder="0" style="position:absolute;top:0px; left:0px;display:none">\
	';
	function $(_sId){return document.getElementById(_sId)}
	this.show = function(){$('dialogBodyBox') ? function(){} : this.init();this.middle('dialogBox');this.shadow();}
	this.reset = function(){$('dialogBox').style.display='none';$('dialogBoxShadow').style.display = "none";$('dialogBoxDivShim').style.display = "none";$('dialogBody').innerHTML = sBody;}
	this.html = function(_sHtml){$("dialogBody").innerHTML = _sHtml;this.show();}
	this.init = function(){
		$('dialogCase') ? $('dialogCase').parentNode.removeChild($('dialogCase')) : function(){};
		var oDiv = document.createElement('span');
		oDiv.id = "dialogCase";
		oDiv.innerHTML = sBox;
		document.body.appendChild(oDiv);
	}
	this.button = function(_sId, _sFuc){
		if($(_sId)){
			$(_sId).style.display = '';
			if($(_sId).addEventListener){
				if($(_sId).act){$(_sId).removeEventListener('click', function(){eval($(_sId).act)}, false);}
				$(_sId).act = _sFuc;
				$(_sId).addEventListener('click', function(){eval(_sFuc)}, false);
			}else{
				if($(_sId).act){$(_sId).detachEvent('onclick', function(){eval($(_sId).act)});}
				$(_sId).act = _sFuc;
				$(_sId).attachEvent('onclick', function(){eval(_sFuc)});
			}
		}
	}
	this.shadow = function(){
		var oShadow = $('dialogBoxShadow');
		var oDialog = $('dialogBox');
		var IfrRef = $('dialogBoxDivShim');
		oShadow.style.position = "absolute";
		oShadow.style.background	= "#000";
		oShadow.style.display	= "";
		oShadow.style.opacity	= "0.2";
		oShadow.style.filter = "alpha(opacity=0)";
		oShadow.style.top = oDialog.offsetTop + 6+"px";
		oShadow.style.left = oDialog.offsetLeft + 6+"px";
		oShadow.style.width = oDialog.offsetWidth+"px";
		oShadow.style.height = oDialog.offsetHeight+"px";
		
		IfrRef.style.width = oDialog.offsetWidth+6+"px";
		IfrRef.style.height = oDialog.offsetHeight+6+"px";
		IfrRef.style.top = oDialog.offsetTop+"px";
		IfrRef.style.left = oDialog.offsetLeft+"px";
		//IfrRef.style.zIndex = DivRef.style.zIndex - 1;
		IfrRef.style.display = "block";
	}
	this.open = function(_sUrl, _sMode){
		this.show();
		if(!_sMode || _sMode == "no" || _sMode == "yes"){
			$("dialogBody").innerHTML = "<iframe id='dialogFrame' width='100%' height='100%' frameborder='0' scrolling='" + _sMode + "'></iframe>";
			$("dialogFrame").src = _sUrl;
		}
	}
	this.event = function(_sMsg, _sOk, _sCancel, _sClose){
		$('dialogFunc').innerHTML = sFunc;
		$('dialogClose').innerHTML = sClose;
		$('dialogBodyBox') == null ? $('dialogBody').innerHTML = sBody : function(){};
		$('dialogMsg') ? $('dialogMsg').innerHTML = _sMsg  : function(){};
		this.show();
		_sOk ? this.button('dialogOk', _sOk) | $('dialogOk').focus() : $('dialogOk').style.display = 'none';
		_sCancel ? this.button('dialogCancel', _sCancel) : $('dialogCancel').style.display = 'none';
		_sClose ? this.button('dialogBoxClose', _sClose) : function(){};
		//_sOk ? this.button('dialogOk', _sOk) : _sOk == "" ? function(){} : $('dialogOk').style.display = 'none';
		//_sCancel ? this.button('dialogCancel', _sCancel) : _sCancel == "" ? function(){} : $('dialogCancel').style.display = 'none';
	}
	this.set = function(_oAttr, _sVal){
		var oShadow = $('dialogBoxShadow');
		var oDialog = $('dialogBox');
		var oHeight = $('dialogHeight');

		if(_sVal != ''){
			switch(_oAttr){
				case 'title':
					$('dialogBoxTitle').innerHTML = _sVal;
					title = _sVal;
					break;
				case 'width':
					oDialog.style.width = _sVal;
					width = _sVal;
					break;
				case 'height':
					oHeight.style.height = _sVal;
					height = _sVal;
					break;
				case 'src':
					if(parseInt(_sVal) > 0){
						$('dialogBoxFace') ? $('dialogBoxFace').src = path + _sVal + '.png' : function(){};
					}else{
						$('dialogBoxFace') ? $('dialogBoxFace').src = _sVal : function(){};
					}
					src = _sVal;
					break;
			}
		}
		this.middle('dialogBox');
		oShadow.style.top = oDialog.offsetTop + 6+"px";
		oShadow.style.left = oDialog.offsetLeft + 6+"px";
		oShadow.style.width = oDialog.offsetWidth+"px";
		oShadow.style.height = oDialog.offsetHeight+"px";
	}
	this.moveStart = function (event, _sId){
		var oObj = $(_sId);
		oObj.onmousemove = mousemove;
		oObj.onmouseup = mouseup;
		oObj.setCapture ? oObj.setCapture() : function(){};
		oEvent = window.event ? window.event : event;
		var dragData = {x : oEvent.clientX, y : oEvent.clientY};
		var backData = {x : parseInt(oObj.style.top), y : parseInt(oObj.style.left)};
		function mousemove(){
			var oEvent = window.event ? window.event : event;
			var iLeft = oEvent.clientX - dragData["x"] + parseInt(oObj.style.left);
			var iTop = oEvent.clientY - dragData["y"] + parseInt(oObj.style.top);
			oObj.style.left = iLeft+"px";
			oObj.style.top = iTop+"px";
			$('dialogBoxShadow').style.left = iLeft + 6+"px";
			$('dialogBoxShadow').style.top = iTop + 6+"px";
			
			$('dialogBoxDivShim').style.left = iLeft+"px";
			$('dialogBoxDivShim').style.top = iTop +"px";
			
			dragData = {x: oEvent.clientX, y: oEvent.clientY};
			

		}
		function mouseup(){
			var oEvent = window.event ? window.event : event;
			oObj.onmousemove = null;
			oObj.onmouseup = null;
			if(oEvent.clientX < 1 || oEvent.clientY < 1 || oEvent.clientX > document.body.clientWidth || oEvent.clientY > document.body.clientHeight){
				oObj.style.left = backData.y +"px";
				oObj.style.top = backData.x +"px";
				$('dialogBoxShadow').style.left = backData.y + 6+"px";
				$('dialogBoxShadow').style.top = backData.x + 6+"px";
				
				$('dialogBoxDivShim').style.left = backData.y +"px";
				$('dialogBoxDivShim').style.top = backData.x +"px";
			}
			oObj.releaseCapture ? oObj.releaseCapture() : function(){};
		}
	}
	this.middle = function(_sId){	
		var theWidth;
		var theHeight;
		if (document.documentElement && document.documentElement.clientWidth) { 
			theWidth = document.documentElement.clientWidth+document.documentElement.scrollLeft*2;;
			theHeight = document.documentElement.clientHeight+document.documentElement.scrollTop*2;; 
		} else if (document.body) { 
			theWidth = document.body.clientWidth;
			theHeight = document.body.clientHeight; 
		}else if(window.innerWidth){
			theWidth = window.innerWidth;
			theHeight = window.innerHeight;
		}
		document.getElementById(_sId).style.display = '';
		document.getElementById(_sId).style.position = "absolute";
		document.getElementById(_sId).style.left = (theWidth / 2) - (document.getElementById(_sId).offsetWidth / 2)+"px";
		//alert(in_ob_useradmin);
		if(document.all||document.getElementById("user_page_top")){
			document.getElementById(_sId).style.top = (theHeight / 2 + document.body.scrollTop) - (document.getElementById(_sId).offsetHeight / 2)+"px";
		}else{
			var sClientHeight = parent ? parent.document.body.clientHeight : document.body.clientHeight;
			var sScrollTop = parent ? parent.document.body.scrollTop : document.body.scrollTop;
			var sTop = -80 + (sClientHeight / 2 + sScrollTop) - (document.getElementById(_sId).offsetHeight / 2);
			document.getElementById(_sId).style.top = sTop > 0 ? sTop : (sClientHeight / 2 + sScrollTop) - (document.getElementById(_sId).offsetHeight / 2)+"px";
			//document.getElementById(_sId).style.top = (theHeight / 2 + document.body.scrollTop) - (document.getElementById(_sId).offsetHeight / 2)+"px";
		}
	}
		
}
BtnOver=function(obj,path){obj.style.backgroundImage = "url("+path+"button3.bmp)";}
BtnOut=function(obj,path){	obj.style.backgroundImage = "url("+path+"button4.bmp)";}


function oAjax( url ,callback)
{
    try{
        this.HttpRequest = null;
        this.Debug  = false;
        this.Url = url;
        this.ContentType = "text/xml";
        this.HttpRequest = this.createXMLHttpRequest();

        if ( this.HttpRequest == null )
        {
            this._debug("XMLHttpRequest create failure!");
            return;
        }

        var xhReq = this.HttpRequest;
        xhReq.onreadystatechange = function (){
            oAjax._OnReadyStateChange( xhReq,callback );
        }

    } catch(e){
       this._debug( "unknow err: " + e.message );
    }
}

/*
 * Get URL resource
 */
oAjax.prototype.Get = function() {

    this.SetContentType( "text/html" );
    this._get();
}

/*
 * Post data to the server
 */
oAjax.prototype.Post = function( arrKey, arrValue ) {

    var data = '';
    this.SetContentType( "application/x-www-form-urlencoded" );
    for( i = 0; i < arrKey.length; i ++)
    {
        data += "&" + escape(arrKey[i]) + "=" + escape(arrValue[i]);
		//data += "&" + arrKey[i] + "=" + arrValue[i];
    }
	//document.write(data);
    data = data.replace(/^&/g, "");
    this._post(data);
}

/*
 * Initialization for oAjax class
 */
oAjax.prototype.Init = function() {
    // initialization
}

/*
 * Change URL for request
 */
oAjax.prototype.SetUrl = function( url ) {
    this.Url = url;
}

/*
 * Set content type for HTTP header before sending request
 */
oAjax.prototype.SetContentType = function( type ) {
    this.ContentType = type;
}

oAjax.prototype.createXMLHttpRequest = function() {

    try { return new ActiveXObject("Msxml2.XMLHTTP");    } catch(e) {}
    try { return new ActiveXObject("Microsoft.XMLHTTP"); } catch(e) {}
    try { return new XMLHttpRequest();                   } catch(e) {}
    return null;
}

/*
 * Debug information for testing
 */
oAjax.prototype._debug = function(message) {

    if ( this.Debug )
    {
        alert(message);
    }
}

/*
 * Process message and data from server
 */
oAjax._OnReadyStateChange = function( xreq, callback ){

    if ( xreq == null )
    {
        return;    }
    
    /*Status is completed, then process result */
    if ( xreq.readyState == 4)
    {
        // OK        
        if ( xreq.status == 200 )
        {
			//alert(xreq.responseText);
          	callback (this.ArrayValue(xreq.responseXML) );                     
        }else{
//			alert('服务器端错误！');
		document.write (xreq.responseText);
		}
    } else {
        // Others
    }
}

oAjax.prototype._SendRequest = function(HttpMethod, data){

    this._debug( 'Send Request ' + HttpMethod + data );
    
    if ( this.HttpRequest != null )
    {
        this.HttpRequest.open(HttpMethod, this.Url, true);

        if ( this.ContentType != null )
        {
            //  <FORM> MIME type: application/x-www-form-urlencoded
            this.HttpRequest.setRequestHeader("Content-Type", this.ContentType);
        }
        this.HttpRequest.send(data);
        return true;
    }
    return false;
}

/* Send GET request to server */
oAjax.prototype._get = function () {

    this._debug( 'GET' );
    return this._SendRequest("GET", null);
}

/* Send POST request and data to server */
oAjax.prototype._post = function (data) {

    this._debug( 'POST' );
    return this._SendRequest("POST", data);
}

oAjax.ArrayValue = function ( xmlobj ) {
    var array = new Array();
    var i = 0;
    var response = xmlobj.getElementsByTagName('Response')[0];
	var element = response.firstChild;
	array[i] = element.firstChild.nodeValue;
	while ( element = element.nextSibling )
	{
		i ++;
		array[i] = element.firstChild.nodeValue;
		}
	return array;
}

if(typeof deconcept=="undefined"){var deconcept=new Object();}
if(typeof deconcept.util=="undefined"){deconcept.util=new Object();}
if(typeof deconcept.SWFObjectUtil=="undefined"){deconcept.SWFObjectUtil=new Object();}
deconcept.SWFObject=function(_1,id,w,h,_5,c,_7,_8,_9,_a,_b){if(!document.getElementById){return;}
this.DETECT_KEY=_b?_b:"detectflash";
this.skipDetect=deconcept.util.getRequestParameter(this.DETECT_KEY);
this.params=new Object();
this.variables=new Object();
this.attributes=new Array();
if(_1){this.setAttribute("swf",_1);}
if(id){this.setAttribute("id",id);}
if(w){this.setAttribute("width",w);}
if(h){this.setAttribute("height",h);}
if(_5){this.setAttribute("version",new deconcept.PlayerVersion(_5.toString().split(".")));}
this.installedVer=deconcept.SWFObjectUtil.getPlayerVersion();
if(c){this.addParam("bgcolor",c);}
var q=_8?_8:"high";
this.addParam("quality",q);
this.setAttribute("useExpressInstall",_7);
this.setAttribute("doExpressInstall",false);
var _d=(_9)?_9:window.location;
this.setAttribute("xiRedirectUrl",_d);
this.setAttribute("redirectUrl","");
if(_a){this.setAttribute("redirectUrl",_a);}};
deconcept.SWFObject.prototype={setAttribute:function(_e,_f){
this.attributes[_e]=_f;
},getAttribute:function(_10){
return this.attributes[_10];
},addParam:function(_11,_12){
this.params[_11]=_12;
},getParams:function(){
return this.params;
},addVariable:function(_13,_14){
this.variables[_13]=_14;
},getVariable:function(_15){
return this.variables[_15];
},getVariables:function(){
return this.variables;
},getVariablePairs:function(){
var _16=new Array();
var key;
var _18=this.getVariables();
for(key in _18){_16.push(key+"="+_18[key]);}
return _16;},getSWFHTML:function(){var _19="";
if(navigator.plugins&&navigator.mimeTypes&&navigator.mimeTypes.length){
if(this.getAttribute("doExpressInstall")){
this.addVariable("MMplayerType","PlugIn");}
_19="<embed type=\"application/x-shockwave-flash\" src=\""+this.getAttribute("swf")+"\" width=\""+this.getAttribute("width")+"\" height=\""+this.getAttribute("height")+"\"";
_19+=" id=\""+this.getAttribute("id")+"\" name=\""+this.getAttribute("id")+"\" ";
var _1a=this.getParams();
for(var key in _1a){_19+=[key]+"=\""+_1a[key]+"\" ";}
var _1c=this.getVariablePairs().join("&");
if(_1c.length>0){_19+="flashvars=\""+_1c+"\"";}_19+="/>";
}else{if(this.getAttribute("doExpressInstall")){this.addVariable("MMplayerType","ActiveX");}
_19="<object id=\""+this.getAttribute("id")+"\" classid=\"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000\" width=\""+this.getAttribute("width")+"\" height=\""+this.getAttribute("height")+"\">";
_19+="<param name=\"movie\" value=\""+this.getAttribute("swf")+"\" />";
var _1d=this.getParams();
for(var key in _1d){_19+="<param name=\""+key+"\" value=\""+_1d[key]+"\" />";}
var _1f=this.getVariablePairs().join("&");
if(_1f.length>0){_19+="<param name=\"flashvars\" value=\""+_1f+"\" />";}_19+="</object>";}
return _19;
},write:function(_20){
if(this.getAttribute("useExpressInstall")){
var _21=new deconcept.PlayerVersion([6,0,65]);
if(this.installedVer.versionIsValid(_21)&&!this.installedVer.versionIsValid(this.getAttribute("version"))){
this.setAttribute("doExpressInstall",true);
this.addVariable("MMredirectURL",escape(this.getAttribute("xiRedirectUrl")));
document.title=document.title.slice(0,47)+" - Flash Player Installation";
this.addVariable("MMdoctitle",document.title);}}
if(this.skipDetect||this.getAttribute("doExpressInstall")||this.installedVer.versionIsValid(this.getAttribute("version"))){
var n=(typeof _20=="string")?document.getElementById(_20):_20;
n.innerHTML=this.getSWFHTML();return true;
}else{if(this.getAttribute("redirectUrl")!=""){document.location.replace(this.getAttribute("redirectUrl"));}}
return false;}};
deconcept.SWFObjectUtil.getPlayerVersion=function(){
var _23=new deconcept.PlayerVersion([0,0,0]);
if(navigator.plugins&&navigator.mimeTypes.length){
var x=navigator.plugins["Shockwave Flash"];
if(x&&x.description){_23=new deconcept.PlayerVersion(x.description.replace(/([a-zA-Z]|\s)+/,"").replace(/(\s+r|\s+b[0-9]+)/,".").split("."));}
}else{try{var axo=new ActiveXObject("ShockwaveFlash.ShockwaveFlash.7");}
catch(e){try{var axo=new ActiveXObject("ShockwaveFlash.ShockwaveFlash.6");
_23=new deconcept.PlayerVersion([6,0,21]);axo.AllowScriptAccess="always";}
catch(e){if(_23.major==6){return _23;}}try{axo=new ActiveXObject("ShockwaveFlash.ShockwaveFlash");}
catch(e){}}if(axo!=null){_23=new deconcept.PlayerVersion(axo.GetVariable("$version").split(" ")[1].split(","));}}
return _23;};
deconcept.PlayerVersion=function(_27){
this.major=_27[0]!=null?parseInt(_27[0]):0;
this.minor=_27[1]!=null?parseInt(_27[1]):0;
this.rev=_27[2]!=null?parseInt(_27[2]):0;
};
deconcept.PlayerVersion.prototype.versionIsValid=function(fv){
if(this.major<fv.major){return false;}
if(this.major>fv.major){return true;}
if(this.minor<fv.minor){return false;}
if(this.minor>fv.minor){return true;}
if(this.rev<fv.rev){
return false;
}return true;};
deconcept.util={getRequestParameter:function(_29){
var q=document.location.search||document.location.hash;
if(q){var _2b=q.substring(1).split("&");
for(var i=0;i<_2b.length;i++){
if(_2b[i].substring(0,_2b[i].indexOf("="))==_29){
return _2b[i].substring((_2b[i].indexOf("=")+1));}}}
return "";}};
deconcept.SWFObjectUtil.cleanupSWFs=function(){if(window.opera||!document.all){return;}
var _2d=document.getElementsByTagName("OBJECT");
for(var i=0;i<_2d.length;i++){_2d[i].style.display="none";for(var x in _2d[i]){
if(typeof _2d[i][x]=="function"){_2d[i][x]=function(){};}}}};
deconcept.SWFObjectUtil.prepUnload=function(){__flash_unloadHandler=function(){};
__flash_savedUnloadHandler=function(){};
if(typeof window.onunload=="function"){
var _30=window.onunload;
window.onunload=function(){
deconcept.SWFObjectUtil.cleanupSWFs();_30();};
}else{window.onunload=deconcept.SWFObjectUtil.cleanupSWFs;}};
if(typeof window.onbeforeunload=="function"){
var oldBeforeUnload=window.onbeforeunload;
window.onbeforeunload=function(){
deconcept.SWFObjectUtil.prepUnload();
oldBeforeUnload();};
}else{window.onbeforeunload=deconcept.SWFObjectUtil.prepUnload;}
if(Array.prototype.push==null){
Array.prototype.push=function(_31){
this[this.length]=_31;
return this.length;};}
var getQueryParamValue=deconcept.util.getRequestParameter;
var FlashObject=deconcept.SWFObject;
var SWFObject=deconcept.SWFObject;