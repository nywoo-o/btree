#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;
#define fout(f, v) f.write((char*)&v, sizeof(v));
#define fin(f, v) f.read((char*)&v, sizeof(v));
typedef vector<pair<int, int>> Node;
//<<leaf���� ����, �ڱ� BID>,  < first or nextNode, -1 >, < key, id >, ... < key, id >>

class Btree {
private:
	const int headerSize = 16;
	int blockSize;
	int maxEntry; //block �ϳ��� ���� entry�� ����
	int root = 1;
	const char* fileName;
	Node emptyNode;
	int BIDs = 1; //���� block id�� ����� �� �ִ� ���� 
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
		fout(out, BIDs); //������ block ID
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
			//write headerSize + (BID)*blockSize - 4; ���ٰ� nextBID ��ֱ�
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
	void clearNode(int BID) { //BID�κ��� block��ġ�� block size��ŭ 0�� �̸� ����� 
		int physPos = headerSize + (BID - 1)*blockSize;
		ofstream out(fileName, ios::out | ios::binary | ios::ate | ios::in);
		out.seekp(physPos);
		int value = 0;
		for (int i = 0; i < blockSize / 4; ++i) {
			fout(out, value);
		}
	}
	Node createNode(bool leaf, int firstValue = 0) { //BID ������ 0���� �� �����س���, �ʱ� ������ ����ִ� �� ��带 return 
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
		Node cur = getNode(root, (BIDs == 2)); //root�� ���� -> root�� leaf���
		while (depth != 0) { //leaf����̸� stop
			depth--;
			int i;
			for (i = 2; i < cur.size(); ++i) {
				if (cur[i].first > key) break; //key���� ���ϴ� ���� ã��
			}
			cur = getNode(cur[i - 1].second, (depth == 0)); //����Ű�� block �о���� 
		}

		return cur; //key���� ���ϴ� leaf node ����
	}
	Node findParent(int BID, int key) { //BID�� ����Ű�� parent block��  ã��
		int depth = this->depth;
		Node cur = getNode(root, 0);
		while (depth != 0) { //leaf����̸� stop
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

		return cur; //key���� ���ϴ� node ����		
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
			Node newRoot = createNode(0, NBID); //original node block id���� �� ������
			newRoot.push_back({ value, NNBID }); //value�� ���ο� ����� block id�� �ϳ��� ��� �߰�
			root = newRoot[0].second;
			setNode(root, newRoot);
			depth++; //���ο� root�� �������ϱ� depth����
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
		 
		//int frontCnt = (maxEntry + 3) / 2; //Pointer������ maxEntry +1, over�� ���� ����ϸ�  +1, ceil�Ϸ��� +1
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
			insert_in_leaf(leafNode, key, value); //�ϴ� �߰�
			
			Node newNode = createNode(true, leafNodeNext); //newnode�� next�� ���� next
			int newNodeBID = newNode[0].second;
			leafNode[1].second = newNodeBID; //���� node�� next�� new node��

			//int frontCnt = (maxEntry + 2) / 2; //�պκ�(leafNode)�� �� ����
			int frontCnt = (maxEntry+1)/2;
			int rearCnt = maxEntry + 1 - frontCnt; //�޺κ�(newNode)�� �� ����

			//maxEntry + 1 + 2 = leafNode.size = 7 // 0 1 2 3 4 5 6
			//frontCnt = 3
			//rearCnt = 2
			for (int i = 2 + frontCnt; i < leafNode.size(); ++i) {
				newNode.push_back(leafNode[i]);
			}
			for (int i = 0; i < rearCnt; ++i) {
				leafNode.pop_back();
			}
			
			clearNode(leafNodeBID); //leafNode file ������ clear
			setNode(leafNodeBID, leafNode); //�ٲ� ������ write
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

		if (depth != 0) { //root�� leaf�� �ƴϸ�
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
//argv[3] - command���� �ٸ�
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
		//key, id�о
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
			//���� ��ã����?? -> ??
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
			Node cur = btree.search(start); //start key�� ������ leaf��带 ������
			while (!cur.empty()) { //null���� �ƴҶ�����
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
