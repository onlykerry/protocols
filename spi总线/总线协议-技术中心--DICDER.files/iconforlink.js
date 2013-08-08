// Add pdf doc zip icon to the link
function fileLinks() {
    var fileLink;
    if (document.getElementsByTagName('a')) {
        for (var i = 0; (fileLink = document.getElementsByTagName('a')[i]); i++) {
            if (fileLink.href.indexOf('.pdf') != -1) {
                fileLink.setAttribute('target', '_blank');
                fileLink.className = 'pdfLink';
            }
            if (fileLink.href.indexOf('.doc') != -1) {
                fileLink.setAttribute('target', '_blank');
                fileLink.className = 'docLink';
            }
            if (fileLink.href.indexOf('.zip') != -1) {
                fileLink.setAttribute('target', '_blank');
                fileLink.className = 'zipLink';
            }
        }
    }
}
window.onload = function() {
    fileLinks();
}


