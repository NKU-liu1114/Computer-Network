// 获取按钮和文本元素
var button1 = document.getElementById("butt1");
var button2 = document.getElementById("butt2");
var button3 = document.getElementById("butt3");
var text1 = document.getElementById("text1");
var text2 = document.getElementById("text2");
var text3 = document.getElementById("text3");
// 事件响应函数
function showMessage(text){
    // 显示文本元素
    if( text.style.visibility=="hidden"){
        text.style.visibility = "visible";
    }
    else{
        text.style.visibility = "hidden";
    }
}

// 为按钮添加点击事件监听器
button1.addEventListener("click", function() {
    showMessage(text1);
});
button2.addEventListener("click", function() {
    showMessage(text2);
});
button3.addEventListener("click", function() {
    showMessage(text3);
});