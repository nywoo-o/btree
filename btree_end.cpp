#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;
#define fout(f, v) f.write((char*)&v, sizeof(v));
#define fin(f, v) f.read((char*)&v, sizeof(v));
typedef vector<pair<int, int>> Node;
//<<leaf인지 여부, 자기 BID>,  < first or nextNode, -1 >, < key, id >, ... < key, id >>

class Btree {
private:
	const int headerSize = 16;
	int blockSize;
	int maxEntry; //block 하나에 들어가는 entry의 개수
	int root = 1;
	const char* fileName;
	Node emptyNode;
	int BIDs = 1; //다음 block id로 사용할 수 있는 숫자 
	int depth = 0;
	
public:
	Btree(const char* fileName, int blockSize) { //if the bin file is never created, we need to write the initial value to it
		this->fileName = fileName;
		this->blockSize = blockSize;
		maxEntry = (blockSize - 4) / 8;

		for (int i = 0; i < maxEntry + 2; ++i) {
			emptyNode.push_back({ 0, 0 });
		}

		ofstream out(fileName, ios::out | ios::binary);
		fout(out, blockSize);
		fout(out, root);
		fout(out, BIDs); //last block ID
		fout(out, depth);
	}
	Btree(const char* fileName) { //if the bin file already exists, we need to read values from it 
		ifstream in(fileName, ios::in | ios::binary);
		fin(in, blockSize);
		fin(in, root);
		fin(in, BIDs);
		fin(in, depth);

		this->fileName = fileName;
		maxEntry = (blockSize - 4) / 8;

		for (int i = 0; i < maxEntry + 2; ++i) {
			emptyNode.push_back({ 0, 0 });
		}

	}
	~Btree() { //when destructing, update the value of root, BIDs, depth 
		ofstream out(fileName, ios::out | ios::binary | ios::ate | ios::in);
		out.seekp(0);
		fout(out, blockSize);
		fout(out, root);
		fout(out, BIDs); //마지막 block ID
		fout(out, depth);
	}
	Node getNode(int BID, bool leaf) {
		if (BID == 0) return Node(); //if null, return empty vector
		int physPos = headerSize + (BID - 1)*blockSize;
		ifstream in(fileName, ios::in | ios::binary);

		Node node;
		node.push_back({ leaf, BID });
		node.push_back({ -1, 0 });
		if (leaf == 1) {
			in.seekg(physPos + blockSize - sizeof(int));
			fin(in, node[1].second);
			in.seekg(physPos);
		}
		else {
			in.seekg(physPos);
			fin(in, node[1].second);
		}

		int key, value;
		for (int i = 0; i < maxEntry; ++i) {
			fin(in, key);
			fin(in, value);
			if (key == 0 && value == 0) break; //if {key, value} == {0, 0}, we think no more entry exists
			node.push_back({ key, value });
		}

		return node;
	}
	void setNode(int BID, Node& node) {
		int physPos = headerSize + (BID - 1)*blockSize;
		ofstream out(fileName, ios::out | ios::binary | ios::ate | ios::in );

		if (node[0].first == 1) {//if leaf node
			//write headerSize + (BID)*blockSize - 4; 에다가 nextBID 써넣기
			out.seekp(physPos + blockSize - sizeof(int));
			fout(out, node[1].second);
			out.seekp(physPos);
		}
		else { //if non-leaf node
			out.seekp(physPos);
			fout(out, node[1].second);
		}

		for (int i = 2; i < node.size(); ++i) {
			fout(out, node[i].first);
			fout(out, node[i].second);
		}
	}
	void clearNode(int BID) { //BID부분의 block위치에 block size만큼 0을 미리 써놓음 
		int physPos = headerSize + (BID - 1)*blockSize;
		ofstream out(fileName, ios::out | ios::binary | ios::ate | ios::in);
		out.seekp(physPos);
		int value = 0;
		for (int i = 0; i < blockSize / 4; ++i) {
			fout(out, value);
		}
	}
	Node createNode(bool leaf, int firstValue = 0) { //BID 영역을 0으로 다 세팅해놓고, 초기 정보만 들어있는 빈 노드를 return 
		clearNode(BIDs);
		Node node;
		node.push_back({ leaf, BIDs });
		node.push_back({ -1, firstValue });
		BIDs++;
		
		return node;
	}
	bool empty() {
		return (BIDs == 1);
	}
	Node search(int key) { 
		int depth = this->depth;
		Node cur = getNode(root, (BIDs == 2)); //root가 유일 -> root가 leaf노드
		while (depth != 0) { //leaf노드이면 stop
			depth--;
			int i;
			for (i = 2; i < cur.size(); ++i) {
				if (cur[i].first > key) break; //key값이 속하는 범위 찾기
			}
			cur = getNode(cur[i - 1].second, (depth == 0)); //가리키는 block 읽어오기 
		}

		return cur; //key값이 속하는 leaf node 리턴
	}
	Node findParent(int BID, int key) { //BID를 가리키는 parent block을  찾기
		int depth = this->depth;
		Node cur = getNode(root, 0);
		while (depth != 0) { //leaf노드이면 stop
			depth--;
			int i;
			for (i = 1; i < cur.size(); ++i) {
				if (cur[i].second == BID) {
					return cur;
				}
				else if (cur[i].first > key) break;
			}
			cur = getNode(cur[i - 1].second, (depth == 0));
		}

		return cur; //key값이 속하는 node 리턴		
	}
	void insert_in_leaf(Node& node, int key, int value) { //insert in the proper position 
		int i;
		for (i = 2; i < node.size(); ++i) {
			if (node[i].first > key) break;
		}
		node.insert(node.begin() + i, { key, value });
	}
	void insert_in_parent(Node& node, int value, Node& newNode) {
		int NBID = node[0].second;
		int NNBID = newNode[0].second;
		if (node[0].second == root) { //if current node is root, we need to create new root
			Node newRoot = createNode(0, NBID); //original node block id값을 맨 앞으로
			newRoot.push_back({ value, NNBID }); //value랑 새로운 노드의 block id를 하나로 묶어서 추가
			root = newRoot[0].second;
			setNode(root, newRoot);
			depth++; //새로운 root가 생겼으니까 depth증가
			return;
		}

		Node par = findParent(NBID, value);
		
		int i;
		for (i = 1; i < par.size(); ++i) {
			if (par[i].second == NBID) {
				break;
			}
		}
		par.insert(par.begin() + i + 1, { value, NNBID }); //insert just after NBID

		if (par.size() - 2 <= maxEntry) {
			setNode(par[0].second, par);
			return; //if there is enough space in parent Node
		}
		
		//if not, we need to split the parent Node
		 
		//int frontCnt = (maxEntry + 3) / 2; //Pointer개수는 maxEntry +1, over된 개수 고려하면  +1, ceil하려면 +1
		int frontCnt = (maxEntry+2)/2;
		
		Node newPar = createNode(0, par[frontCnt+1].second);
		int upValue = par[frontCnt + 1].first;
		for (int i = frontCnt + 2; i < par.size(); ++i) {
			newPar.push_back(par[i]);
		}
		for (int i = par.size() - 1; i >= frontCnt + 1; --i) par.pop_back();
		
		//update block entry values
		clearNode(par[0].second);
		setNode(par[0].second, par); 
		setNode(newPar[0].second, newPar);
		insert_in_parent(par, upValue, newPar);
	}
	bool insert(int key, int value) {
		//if empty create an empty leaf node L, which is also the root
		Node leafNode;
		if (empty()) leafNode = createNode(1);
		else leafNode = search(key);

		int leafNodeBID = leafNode[0].second;
		int leafNodeNext = leafNode[1].second;

		//if (L has less than n-1 key values)
		if (leafNode.size() - 2 < maxEntry) {
			insert_in_leaf(leafNode, key, value);
			setNode(leafNodeBID, leafNode);
		}
		else { //full
			insert_in_leaf(leafNode, key, value); //일단 추가
			
			Node newNode = createNode(true, leafNodeNext); //newnode의 next에 원래 next
			int newNodeBID = newNode[0].second;
			leafNode[1].second = newNodeBID; //원래 node의 next는 new node로

			//int frontCnt = (maxEntry + 2) / 2; //앞부분(leafNode)에 들어갈 개수
			int frontCnt = (maxEntry+1)/2;
			int rearCnt = maxEntry + 1 - frontCnt; //뒷부분(newNode)에 들어갈 개수

			//maxEntry + 1 + 2 = leafNode.size = 7 // 0 1 2 3 4 5 6
			//frontCnt = 3
			//rearCnt = 2
			for (int i = 2 + frontCnt; i < leafNode.size(); ++i) {
				newNode.push_back(leafNode[i]);
			}
			for (int i = 0; i < rearCnt; ++i) {
				leafNode.pop_back();
			}
			
			clearNode(leafNodeBID); //leafNode file 영역을 clear
			setNode(leafNodeBID, leafNode); //바뀐 값으로 write
			setNode(newNodeBID, newNode);
			insert_in_parent(leafNode, newNode[2].first, newNode);
		}

		return true;
	}
	void printLevel(ofstream& out) {
		Node rootNode = getNode(root, (BIDs == 2));

		out << "<0>\n";
		for (int i = 2; i < rootNode.size(); ++i) {
			out << rootNode[i].first;
			if (i != rootNode.size() - 1) out << ',';
		}
		out << '\n';

		if (depth != 0) { //root가 leaf가 아니면
			out << "<1>\n";
			Node tmp;
			for (int i = 1; i < rootNode.size(); ++i) {
				tmp = getNode(rootNode[i].second, (depth == 1));
				for (int j = 2; j < tmp.size(); ++j) {
					out << tmp[j].first;
					if (j != tmp.size() - 1 || i != rootNode.size() - 1) out << ',';
				}
			}
			out << '\n';
		}
	}
};

//argc: argument count, argv : argument vector
//argv[1] - command
//argv[2] - binary file
//argv[3] - command마다 다름
int main(int argc, char* argv[]) {
	char command = argv[1][0];
	switch (command) {
	case 'c': {
		Btree btree(argv[2], atoi(argv[3]));
		break; 
	}
	case 'i': {
		Btree btree(argv[2]);
		ifstream input(argv[3]);
		//key, id읽어서
		int key, value;
		char c;
		while (input >> key >> c >> value) {
			btree.insert(key, value);
		}
		break; 
	}
	case 's': {
		Btree btree(argv[2]);
		ifstream input(argv[3]);
		ofstream output(argv[4]);
		int key;
		while (input >> key) {
			Node cur = btree.search(key);
			for (int i = 2; i < cur.size(); ++i) {
				if (cur[i].first == key) {
					output << key << ',' << cur[i].second << '\n';
					break;
				}
			}
			//만약 못찾으면?? -> ??
		}

		break;
	}
	case 'r': {
		Btree btree(argv[2]);
		ifstream input(argv[3]);
		ofstream output(argv[4]);
		int start, end;
		char c;
		while (input >> start >> c >> end) {
			Node cur = btree.search(start); //start key를 포함한 leaf노드를 가져옴
			while (!cur.empty()) { //null값이 아닐때까지
				bool findEnd = false;
				for (int i = 2; i < cur.size(); ++i) {
					if (cur[i].first >= start && cur[i].first <= end) {
						output << cur[i].first << ',' << cur[i].second << '\t';
					}
					else if (cur[i].first > end) findEnd = 1;
				}
				if (findEnd) {
					break;
				}
				else cur = btree.getNode(cur[1].second, true);
			}
			output << '\n';
		}
		break;
	}
	case 'p': {
		Btree btree(argv[2]);
		ofstream out(argv[3]);
		btree.printLevel(out);
		break; }
	default: break;
	}
	
}
