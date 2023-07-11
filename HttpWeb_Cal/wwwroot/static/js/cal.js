function $(id) { return document.getElementById(id); }
var str;
function display(str0)	//显示到文本框
{
	str = document.getElementById("text");
	str.value = str.value + str0;
}
function back()		//退格
{
	str = document.getElementById("text");
	str.value = str.value.substring(0, str.value.length - 1);
}
function reset()	//清除
{
	str = document.getElementById("text");
	str.value = "";
}
