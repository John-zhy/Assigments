package back;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

class Pit
{
	protected Pit next = null;
	protected int val = 3;
	protected int index;
	protected int owner;
}
class MancalaPit extends Pit{}
class AlphaBetaPruning{
	private int mAlpha = Integer.MIN_VALUE;
	private int mBeta = Integer.MAX_VALUE;
	AlphaBetaPruning(AlphaBetaPruning previous)
	{
		if(previous != null){
			mAlpha = previous.mAlpha;
			mBeta = previous.mBeta;
		}
	}
	public boolean canBePruned(Node node)
	{
		if(node instanceof MinNode){
			if(node.mVal <= mAlpha) return true;
			mBeta = Integer.min(mBeta, node.mVal);
		}else{
			if(node.mVal >= mBeta) return true;
			mAlpha = Integer.max(mAlpha, node.mVal);	
		}
		return false;
	}
	private String intToString(int val){
		String valstr = (val == Integer.MAX_VALUE)? "Infinity" : (""+val);
		valstr = (val == Integer.MIN_VALUE)? "-Infinity" : valstr;
		return valstr;
	}
	@Override
	public String toString()
	{
		return "," + intToString(mAlpha) + "," + intToString(mBeta); 
	}
}
class Global
{
	static int CutoffDepth;
}
abstract class Node
{
	AlphaBetaPruning mPruning;
	final int mCurDepth;
	int mVal;
	final String mName;
	final State mState;
	State mBestState;
	boolean isUpdated = false;
	abstract boolean canUpdate(Node n);
	abstract void setDefaultVal();
	Node(String _name, int _curDepth, State _state, final AlphaBetaPruning  previous)
	{
		mName = _name;
		mCurDepth = _curDepth;
		mBestState = mState = _state;
		setDefaultVal();
		if(!hasNextNode()) mVal = _state.evaluation();
		if(previous != null) mPruning = new AlphaBetaPruning(previous);	
	}
	boolean isLeaf()
	{
		return (mCurDepth == Global.CutoffDepth) && (!mState.hasFreeMove());
	}
	boolean hasNextNode()
	{
		if(mState.isGameOver()) return false;
		return (mCurDepth < Global.CutoffDepth)||( (mCurDepth == Global.CutoffDepth) && (mState.hasFreeMove()));
	}
	abstract Node nextLevelMidNode(String name, State nextState);
	abstract Node nextLevelBottomNode(String name, State nextState);
	abstract Node sameLevelMidNode(String name, State nextState);
	abstract Node sameLevelBottomNode(String name, State nextState);
	Node nextBottomNode(String name, State nextState)
	{
		return (!mState.hasFreeMove()) ? nextLevelBottomNode(name, nextState) : sameLevelBottomNode(name, nextState);
	}
	Node nextMidNode(String name, State nextState)
	{
		return (!mState.hasFreeMove()) ? nextLevelMidNode(name, nextState) : sameLevelMidNode(name, nextState);
	}
	Node nextNode(int i)
	{
		State nextState = mState.nextState(i);
		if(nextState == null) return null;
		String nextname = ((mState.curPlayer) == 0? "B": "A")+ (i+2);
		Node next = null;
		if(!nextState.hasFreeMove()){
			next = nextBottomNode(nextname, nextState);
		}else{ 
			next = nextMidNode(nextname, nextState);
		}
		return next;
	}
	void updateNextBestState()
	{
		if(!hasNextNode()) return;
		for(int i = 0; i < mState.board[0].length; ++i){
			Node next = nextNode(i);
			if(next == null) continue;
			next.updateNextBestState();
			tryUpdate(next);
			boolean canBePruned =  canBePruned();
			if(canBePruned) return;
		}
	}
	boolean canBePruned()
	{
		return mPruning != null && mPruning.canBePruned(this);
	}
	void tryUpdate(Node n)
	{
		boolean isEndNodeOfLevel = !n.mState.hasFreeMove();
		if((!isUpdated && mState.hasFreeMove()) || canUpdate(n)){
			isUpdated = true;
			mVal = n.mVal;
			mBestState = (isEndNodeOfLevel)? n.mState: n.mBestState;
		}
	}
	private String intToString(int val){
		String valstr = (val == Integer.MAX_VALUE)? "Infinity" : (""+val);
		valstr = (val == Integer.MIN_VALUE)? "-Infinity" : valstr;
		return valstr;
	}
	public String toString()
	{
		String res = mName+"," + mCurDepth + "," + intToString(mVal);
		if(mPruning != null){
			res += mPruning; 
		}
		return res;
	}
}
class RootNode extends MaxNode
{
	RootNode(State _state, AlphaBetaPruning pruning) {
		super("root", 0, _state, pruning);
	}
}
class MinNode extends Node
{
	MinNode(String _name, int _curDepth, State _state, AlphaBetaPruning pruning) {
		super(_name, _curDepth, _state, pruning);
	}
	@Override
	boolean canUpdate(Node n)
	{
		return n.mVal < mVal;
	}
	@Override
	Node nextLevelBottomNode(String name, State nextState)
	{
		return new MaxNode(name, mCurDepth + 1, nextState, mPruning);
	}
	@Override
	Node sameLevelMidNode(String name, State nextState)
	{
		return new MinNode(name, mCurDepth, nextState, mPruning);
	}
	@Override
	void setDefaultVal() 
	{
		mVal = Integer.MAX_VALUE;
	}
	@Override
	Node nextLevelMidNode(String name, State nextState)
	{
		return new MinNode(name, mCurDepth + 1, nextState, mPruning);
	}
	@Override
	Node sameLevelBottomNode(String name, State nextState) 
	{
		return new MaxNode(name, mCurDepth, nextState, mPruning);
	}
}

class MaxNode extends Node//both intermediate node and bottom node.
{
	MaxNode(String _name, int _curDepth, State _state,
			AlphaBetaPruning pruning) {
		super(_name, _curDepth, _state, pruning);
	}
	@Override
	boolean canUpdate(Node n)
	{
		return n.mVal > mVal; 
	}
	@Override
	Node nextLevelBottomNode(String name, State nextState)
	{
		return new MinNode(name, mCurDepth + 1, nextState, mPruning);
	}
	@Override
	Node sameLevelMidNode(String name, State nextState)
	{
		return new MaxNode(name, mCurDepth,  nextState, mPruning);
	}
	@Override
	void setDefaultVal()
	{
		mVal = Integer.MIN_VALUE; 
	}
	@Override
	Node nextLevelMidNode(String name, State nextState) {
		return new MaxNode(name, mCurDepth + 1, nextState, mPruning);
	}
	@Override
	Node sameLevelBottomNode(String name, State nextState) {
		return new MinNode(name, mCurDepth,  nextState, mPruning);
	}
}
class State
{
	protected Pit [][] board = new Pit[2][];// board[0] is bottom
	protected MancalaPit []stone = new MancalaPit[2];
	protected int curPlayer;
	protected int myPlayer;
	private boolean mHasFreeMove = false;
	private boolean isGameOver = false;
	public boolean hasFreeMove(){
		return mHasFreeMove;
	}
	@Override
	protected State clone(){
		State copy = new State(board[0].length, stone[0].val, stone[1].val, curPlayer, myPlayer);
		for(int i = 0; i < 2; ++i){
			for(int j = 0; j < board[i].length; ++j){
				copy.board[i][j].val = board[i][j].val;
			}
		}
		return copy;
	}
	State(int[][] boardval, int stone1, int stone2, int _curplayer, int _myPlayer){
		this(boardval[0].length, stone1, stone2, _curplayer, _myPlayer);
		for(int i = 0; i < 2; ++i){
			for(int j = 0; j < boardval[i].length; ++j){
				board[i][j].val = boardval[i][j];
			}
		}
	}
	private State(int size, int stone1, int stone2, int _curplayer, int _myPlayer)
	{
		board[0] = new Pit[size];
		board[1] = new Pit[size];
		stone[0] = new MancalaPit();
		stone[1] = new MancalaPit();
		stone[0].owner = 0;
		stone[1].owner = 1;
		stone[0].val = stone1; 
		stone[1].val = stone2;
		curPlayer = _curplayer;
		myPlayer = _myPlayer;
		for(int j = 0; j < 2; ++j){
			for(int i = 0; i < size; ++i){
				board[j][i] = new Pit();
				board[j][i].index = i;
				board[j][i].owner = j;
			}
		}		
		for(int i = 0; i+1 < size; ++i){
			board[0][i].next = board[0][i+1];
			
		}		
		board[0][size-1].next = stone[0];
		stone[0].next = board[1][size-1];
		for(int i = size-1; i > 0; --i){
			board[1][i].next = board[1][i-1];
		}		
		board[1][0].next = stone[1];
		stone[1].next = board[0][0];
	}
	int pickStones(int i)
	{
		int n = board[curPlayer][i].val;
		board[curPlayer][i].val = 0;
		return n;
	}
	private void shortcutNCircle(int n)
	{
		if(n <= 0) return;
		for(int j = 0; j < 2; ++j){
			for(int i = 0; i < board[0].length; ++i){
				board[j][i].val += n; 
			}
		}
		stone[curPlayer].val += n;
	}
	private Pit nextPit(Pit cur)
	{
		Pit next = cur.next;
		if(next instanceof MancalaPit && next.owner != curPlayer){
			next = next.next;
		}	
		return next;
	}
	private Pit placeStones(int pitindex, int n){
		int totalSize = board[0].length*2+1;
		int circleNum = n/totalSize;
		shortcutNCircle(circleNum);
		n = n%totalSize;
		Pit last = board[curPlayer][pitindex];
		while(n-- > 0){
			last = nextPit(last);	
			++last.val;
		}
		return last;
	}
	int opponent(int p){
		return (p == 0) ? 1 : 0;
	}
	State nextState(int pitindex)
	{
		State next = clone();
		if(next.toNextState(pitindex)){
			return next;
		}
		return null;
	}
	private boolean tryGameOver(){
		if(isGameOver()){
			afterGameOver();
			return true;
		}
		return false;
	}
	boolean isGameOver()
	{
		if(isGameOver) return true;
		int [] sum = new int[]{0 ,0};
		for(int j = 0; j < 2; ++j){
			for(int i = 0; i < board[0].length; ++i){
				sum[j] += board[j][i].val; 
			}
		}
		return (sum[0] == 0) || (sum[1] == 0);
	}
	private void afterGameOver()
	{
		for(int j = 0; j < 2; ++j){
			for(int i = 0; i < board[0].length; ++i){
				stone[j].val += board[j][i].val; 
				board[j][i].val = 0;
			}
		}
		isGameOver = true;
		mHasFreeMove = false;
	}
	private boolean toNextState(int pitindex)
	{
		int n = pickStones(pitindex);
		if(n <= 0) return false;
		Pit last = placeStones(pitindex, n);
		mHasFreeMove = last instanceof MancalaPit;
		if(!(mHasFreeMove) ){
			if(last.val == 1 && last.owner == curPlayer /*&& board[opponent(curPlayer)][last.index].val > 0*/){
				stone[curPlayer].val += last.val + board[opponent(curPlayer)][last.index].val;
				last.val = 0;
				board[opponent(curPlayer)][last.index].val = 0;
			}
			curPlayer = opponent(curPlayer);
		}
		tryGameOver();
		return true;
	}
	Integer evaluation(){
		return stone[myPlayer].val - stone[opponent(myPlayer)].val;
	}
	public String toString(){
		String res = "";
		for(int j = 1; j >= 0; --j){
			res += board[j][0].val;
			for(int i = 1; i < board[j].length; ++i){
				res +=  " " + board[j][i].val;
			}
			res += "\n";
		}
		res += stone[1].val + "\n";
		res += stone[0].val;
		return res;
	}
}
public class mancala 
{
	State mRoot;
	int mTask;
	int mCutoffDepth;
	mancala(String fileName) throws NumberFormatException, IOException
	{
		FileInputStream in = new FileInputStream(fileName);
		BufferedReader br = new BufferedReader(new InputStreamReader(in));
		mTask = Integer.parseInt(br.readLine());
		int myPlayer = Integer.parseInt(br.readLine()) - 1;
		mCutoffDepth = Integer.parseInt(br.readLine());
		String [] boardStr0 = br.readLine().split(" ");
		String [] boardStr1= br.readLine().split(" ");
		int stone2 = Integer.parseInt(br.readLine());
		int stone1 = Integer.parseInt(br.readLine());
		br.close();
		String [][] boardStr = {boardStr1, boardStr0};
		int [][] boardval = new int[2][boardStr0.length];
		for(int j = 0; j < 2; ++j){
			for(int i = 0; i < boardval[0].length; ++i){
				boardval[j][i] = Integer.parseInt(boardStr[j][i]);
			}
		}
		mRoot = new State(boardval, stone1, stone2, myPlayer, myPlayer);
	}
	public State searchBestNext() throws IOException 
	{
		if(mTask == 1) return greedy(mRoot);
		if(mTask == 2) return minimax(mRoot, mCutoffDepth);
		if(mTask == 3) return alphaBetaPruning(mRoot, mCutoffDepth);
		return null; 
	}
	private State search(State state, boolean enabledLog, boolean enabledPruning) throws IOException {

		Global.CutoffDepth = mCutoffDepth;
		RootNode root = new RootNode(state, enabledPruning? new AlphaBetaPruning(null) : null);
		root.updateNextBestState();
		FileOutputStream out = new FileOutputStream("next_state.txt");
		BufferedWriter nextStateWriter = new BufferedWriter(new OutputStreamWriter(out) );	
		nextStateWriter.write(root.mBestState.toString());
		nextStateWriter.newLine();
		nextStateWriter.close();
		return root.mBestState;
	}
	private State minimax(State state, int depth) throws IOException
	{
		return search(state, false, false);
	}
	private State alphaBetaPruning(State state, int depth) throws IOException
	{
		return search(state, false, true);
	}
	private State greedy(State state) throws IOException
	{
		mCutoffDepth = 1;
		return search(state, false, false);
	}
	public static void main(String [] args) throws IOException
	{
		mancala m = new mancala(args[1]);
		m.searchBestNext();
	}
}