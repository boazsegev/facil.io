rm combine.js
rm combine.css
curl http://code.jquery.com/jquery-1.8.2.js >> combine.js
curl http://www.google.com/jsapi >> combine.js
curl http://www.google.com/uds/api/visualization/1.0/dca88b1ff7033fac80178eb526cb263e/format+en,default+en,ui+en,table+en,corechart+en.I.js >> combine.js
curl http://www.google.com/uds/api/visualization/1.0/dca88b1ff7033fac80178eb526cb263e/ui+en,table+en.css >> combine.css
curl http://ajax.googleapis.com/ajax/static/modules/gviz/1.0/core/tooltip.css >> combine.css
curl http://jquery-csv.googlecode.com/git/src/jquery.csv.js >> combine.js
curl http://maxcdn.bootstrapcdn.com/bootstrap/3.2.0/js/bootstrap.min.js >> combine.js
curl http://maxcdn.bootstrapcdn.com/bootstrap/3.2.0/css/bootstrap.min.css >> combine.css
curl http://maxcdn.bootstrapcdn.com/bootstrap/3.2.0/css/bootstrap-theme.min.css >> combine.css
