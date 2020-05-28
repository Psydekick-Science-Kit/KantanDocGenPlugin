Node.prototype.query = function(expression, namespaceResolver){
	let doc = this.ownerDocument || this;
	let result = doc.evaluate(expression, this, namespaceResolver);

	let iteratorTypes = [XPathResult.UNORDERED_NODE_ITERATOR_TYPE, XPathResult.ORDERED_NODE_ITERATOR_TYPE];
	if(iteratorTypes.indexOf(result.resultType) > -1){
		let nodes = [];
		let node = result.iterateNext();
		while(node){
			nodes.push(node);
			node = result.iterateNext();
		}
		return nodes;
	}

	let snapshotTypes = [XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE];
	if(snapshotTypes.indexOf(result.resultType) > -1){
		let nodes = [];
		for(var i=0; i<result.snapshotLength; i++) {
			nodes.push(result.snapshotItem(i));
		}
		return nodes;
	}

	return result;
};

var DocBrowser = {
	rootURI: './',

	onPageLoad: function(rootURI){
		DocBrowser.rootURI = rootURI;

		let ancestorCount = (DocBrowser.rootURI.match(/.\/./g) || []).length;
		let pathParts = window.location.pathname.split('/');
		pathParts.reverse();
		let classID = pathParts[ancestorCount+1];

		let expandos = document.query('//*[@class="sidebar"]/details');
		for(let expando of expandos){
			let expandoHref = expando.query('.//a')[0].href;
			if(window.location.href.startsWith(expandoHref)){
				expando.open = true;
			}
		}
	},

	toggleClassSidebar: function (element, classID){
		let listElement = element.query('ul')[0];
		if(listElement.childElementCount == 1){
			let addItem = function(text, href, classname){
				let link = document.createElement('a');
				link.href = href;
				link.append(document.createTextNode(text));

				let listItem = document.createElement('li');
				listItem.className = classname;
				listItem.appendChild(link);
				listElement.appendChild(listItem);

				return listItem;
			}

			loadingNode = addItem('Loading...', '', 'loading');

			fetch([DocBrowser.rootURI, classID, '/index.xml'].join(''))
				.then(response => response.text())
				.then(str => (new window.DOMParser()).parseFromString(str, 'text/xml'))
				.then(data => {
					let funcNodes = data.query('/class/functions/*');
					funcNodes.sort(DocBrowser.makeChildNodeTextComparator('name'));

					for (const funcNode of funcNodes) {
						let funcID = funcNode.query('id')[0].textContent;
						let funcName = funcNode.query('name')[0].textContent;

						addItem(funcName, DocBrowser.rootURI+classID+'/'+funcID+'/', 'function');
					}

					loadingNode.parentNode.removeChild(loadingNode);

					return true;
				});
		}
	},

	makeChildNodeTextComparator: function(childNodeName){
		return function(a, b){
			a = a.query(childNodeName)[0].textContent;
			b = b.query(childNodeName)[0].textContent;
			if(a === b){
				return 0;
			}
			return a > b ? 1 : -1;
		};
	},
};

if(window.location.href.endsWith('index.xml')){
	window.history.replaceState({}, document.title, "./");
}
