Node.prototype.query = function(expression, namespaceResolver){
	let nodes = [];

	let doc = this.ownerDocument || this;
	let result = doc.evaluate(expression, this, namespaceResolver, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);

	for(var i=0; i<result.snapshotLength; i++) {
		nodes.push(result.snapshotItem(i));
	}

	return nodes;
};

var DocBrowser = {
	rootURI: './',

	onPageLoad: function(rootURI){
		DocBrowser.rootURI = rootURI;
		console.log(window.location);
		console.log(DocBrowser.rootURI);
	},

	toggleClassSidebar: function (element, classID){
		if(element.childElementCount == 1){
			let loadingNode = document.createTextNode("⏳");
			element.appendChild(loadingNode);

			fetch([DocBrowser.rootURI, classID, '/index.xml'].join(''))
				.then(response => response.text())
				.then(str => (new window.DOMParser()).parseFromString(str, "text/xml"))
				.then(data => {
					let listElement = document.createElement('ul');
					let funcNodes = data.query('/class/functions/*');
					funcNodes.sort(DocBrowser.makeChildNodeTextComparator('name'));

					for (const funcNode of funcNodes) {
						let funcID = funcNode.query('id')[0].textContent;
						let funcName = funcNode.query('name')[0].textContent;

						let link = document.createElement('a');
						link.href = DocBrowser.rootURI+classID+'/'+funcID+'/';
						link.append(document.createTextNode(funcName));

						let listItem = document.createElement('li');
						listItem.appendChild(link);

						listElement.appendChild(listItem);
					}

					loadingNode.parentNode.removeChild(loadingNode);
					element.appendChild(listElement);

					return true;
				});
		}
	},

	makeChildNodeTextComparator: function(childNodeName){
		return function(a, b){
			return a.query(childNodeName)[0].textContent > b.query(childNodeName)[0].textContent;
		};
	},
};

window.history.pushState({}, document.title, "./");
