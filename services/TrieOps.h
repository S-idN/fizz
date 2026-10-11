struct TrieNode {
    TrieNode *children[27];
    bool isEnd;

    TrieNode() {
	isEnd = false;
    }
};

void initializeTrie();

void appendTrie(char appChar);
