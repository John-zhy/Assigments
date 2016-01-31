import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Scanner;

class Substitution
{
	HashMap<String, String> mSubst = new HashMap<String, String>();
	Substitution(){}
	Substitution(Substitution s)
	{
		s.mSubst.forEach((k, v) -> {mSubst.put(k, v);});
	}
	public String toString()
	{
		return mSubst.toString();
	}
}

class Clause
{
	Clause(){}
	Clause(Clause c)
	{
		mPredicate = c.mPredicate;
		c.mVariables.forEach((v) -> {mVariables.add(v);});
	}
	Clause(String clause)
	{
		String [] split1 = clause.split("\\(");
		mPredicate = split1[0];
		String vars[] = (split1[1].split("\\)")[0]).split(",");
		for(int i = 0; i < vars.length; ++i){
			mVariables.add(vars[i]);
		}
		//System.out.println(this);
	}
	public String toString(){
		StringBuilder sb = new StringBuilder();
		sb.append(mPredicate);
		sb.append('(');
		mVariables.forEach((v) -> {sb.append(v + ",");});
		sb.replace(sb.length() - 1, sb.length(), ")");
		return sb.toString();
	}
	String mPredicate;
	List<String> mVariables = new ArrayList<String>();
	boolean isConstant()
	{
		for(String v : mVariables){
			if(Character.isLowerCase(v.charAt(0))){
				return false;
			}
		}
		return true;
	}
}

class Rule
{
	Clause mRight;
	List<Clause> mLefts = null;
	public String toString(){
		StringBuilder sb = new StringBuilder();
		if(mLefts != null && !mLefts.isEmpty()){
			mLefts.forEach((v) -> {sb.append(v + " ^ ");});
			sb.replace(sb.length() - 3, sb.length(), "");
			sb.append(" => ");
		}
		sb.append(mRight);
		return sb.toString();
	}
}

class KnowledgeBase
{
	HashMap<String, List<Rule>> mKB = new HashMap<String, List<Rule>>();
	
	public boolean canBeInfered(Clause goal)
	{
		reset();
		return backChainingOr(goal, new Substitution()) != null;
	}
	private HashSet<String> mSearchedRight = new HashSet<String>();
	private boolean isCircle(Clause c)
	{
		return mSearchedRight.contains(c.toString());
	}
	private void tryAddConstant(Clause c)
	{
		if(c.isConstant() && !mSearchedRight.contains(c.toString())){
			mSearchedRight.add(c.toString());
		}
	}
	private void tryRemoveConstant(Clause c){
		if(mSearchedRight.contains(c.toString())){
			mSearchedRight.remove(c.toString());
		}
	}
	private List<Substitution> backChainingOr(Clause goal, final Substitution theta)
	{
		LinkedList<Substitution> res = new LinkedList<Substitution>();
		List<Rule> rules = mKB.get(goal.mPredicate);
		if(rules != null){
			for(Rule rule : rules){
				//System.out.println("goal\t" + goal + "\ttheta =\t" + theta);
				if(isCircle(goal)){
					continue;
				}
				Rule standardizedRule = standardizeVariables(rule);
				//System.out.println(goal + "\t standardizedRule\t" + standardizedRule);
				tryAddConstant(goal);
				//System.out.println("unify\t" + unify(standardizedRule.mRight, goal, new Substitution(theta)));
				List<Substitution> theta1s = backChainingAnd(standardizedRule.mLefts,unify(standardizedRule.mRight, goal, new Substitution(theta)));
				/*if(goal.isConstant() && mSearchedRight.contains(goal.toString())){
					mSearchedRight.remove(goal.toString());
				}*/
				tryRemoveConstant(goal);
				if(theta1s != null){
					theta1s.forEach((theta1) -> {res.add(theta1);});
					break;
				}
			}
		}
		//System.out.println("backChainingOr\t" + res);
		return res.isEmpty() ? null : res;
	}
	private List<Substitution> backChainingAnd(List<Clause> goals, final Substitution theta)
	{
		//System.out.println("Rules\t" + theta);
		if(theta == null){
			return null;
		}
		
		LinkedList<Substitution> res = new LinkedList<Substitution>();
		if(goals.isEmpty()){
			if(theta != null){
				res.add(theta);
			}
			return res;
		}
		Clause first = goals.remove(0);
		
		List<Substitution> theta1s = backChainingOr(substitute(theta, first), theta);
		if(theta1s !=  null){
			for(Substitution theta1: theta1s){
				List<Substitution> theta2s = backChainingAnd(goals, new Substitution(theta1));
				if(theta2s != null){
					theta2s.forEach((theta2) -> {res.add(theta2);});
				}
			}
		}
		//System.out.println("backChainingAnd\t" + res);
		return res.isEmpty() ? null : res;
	}
	
	private Substitution unify(Clause mRight, Clause goal, Substitution theta) 
	{
		LinkedList<String> rhs = new LinkedList<String>(mRight.mVariables);
		LinkedList<String> gs= new LinkedList<String>(goal.mVariables);
		return unify(rhs, gs, theta);
	}
	Clause substitute(Substitution theta, Clause c)
	{
		Clause q = new Clause();
		q.mPredicate = c.mPredicate;
		c.mVariables.forEach(
				(v) -> {
					String subst = theta.mSubst.get(v);
					q.mVariables.add(subst != null ? subst : v);
			});
		//System.out.println(q.mVariables);
		return q;
	}
	private boolean isVariable(String s)
	{
		return s != null && Character.isLowerCase(s.charAt(0));
	}
	Substitution unify(List<String> x, List<String> y, Substitution theta)
	{
		if(theta == null){
			return null;
		}
		//System.out.println(x.toString() + y + theta.mSubst);
		if(x.equals(y)){
			return theta;
		}
		if(x.size() == 1 && y.size() == 1){
			String xv = x.remove(0);
			String yv = y.remove(0);
			if(isVariable(xv)){
				return unifyVar(xv, yv, theta);
			}
			if(isVariable(yv)){
				return unifyVar(yv, xv, theta);
			}
			if(xv.equals(yv)){
				return new Substitution();
			}
		}
		if(x.size() > 1 && y.size() > 1){
			List<String> firstx = new LinkedList<String>(); 
			firstx.add(x.remove(0));
			List<String> firsty = new LinkedList<String>(); 
			firsty.add(y.remove(0));
			return unify(x, y, unify(firstx, firsty, theta));
		}
		return null;
	}
	Substitution unifyVar(String var, String x, Substitution theta)
	{
		if(theta.mSubst.containsKey(var)){
			LinkedList<String> val = new LinkedList<String>();
			val.add(theta.mSubst.get(var));
			LinkedList<String> xl = new LinkedList<String>();
			xl.add(x);
			return unify(val, xl, theta);
		}
		if(theta.mSubst.containsKey(x)){
			unifyVar(x, var, theta);
		}
		theta.mSubst.put(var, x);
		//System.out.println("theta = \t" +  theta);
		return theta;
	}
	private void addRule(Scanner scanner)
	{
		String line = scanner.nextLine();
		String[] pq = line.split(" => ");
		Rule r = new Rule();
		r.mRight = new Clause(pq[pq.length -1]);
		if(!mKB.containsKey(r.mRight.mPredicate)){
			mKB.put(r.mRight.mPredicate, new LinkedList<Rule>());
		}
		if(pq.length > 1){
			r.mLefts = new LinkedList<Clause>();
			String[] ps = pq[0].split(" \\^ ");
			for(int i = 0; i < ps.length; ++i){
				r.mLefts.add(new Clause(ps[i]));
			}
		}
		if(r.mLefts == null){
			mKB.get(r.mRight.mPredicate).add(0, r);;
		}else{
			mKB.get(r.mRight.mPredicate).add(r);
		}
	}
	void addKB(Scanner scanner, int numKB)
	{
		while(numKB-->0){
			addRule(scanner);
		}
	}
	long mVariableCount = 0;
	private String newVariableName(){
		return "x" + mVariableCount++;
	}
	HashMap<String, Clause> mStandardizedClause = new HashMap<String, Clause>();
	private void reset() {
		mStandardizedClause = new HashMap<String, Clause>();
		mSearchedRight = new HashSet<String>();
		mVariableCount = 0;
	}
	private boolean isStandardized(Clause c)
	{
		return false;
		//return mStandardizedClause.containsKey(c.mPredicate);
	}
	private void addStandardized(Clause c)
	{
		mStandardizedClause.put(c.mPredicate, c);
	}
	private Clause getStandardized(Clause c)
	{
		return mStandardizedClause.get(c.mPredicate);
	}
	private void varReplacementTable(Clause c, HashMap<String, String> table)
	{
		if(isStandardized(c)){
			Clause sc = getStandardized(c);
			for(int i = 0; i < c.mVariables.size(); ++i){
				String v = c.mVariables.get(i);
				//System.out.println(sc + "\t key \t" + mStandardizedClause);
				if(!table.containsKey(v)){
					table.put(v, sc.mVariables.get(i));
				}
			}
		}else{
			c.mVariables.forEach((v)->{
				if(!table.containsKey(v)){
					table.put(v, isVariable(v) ? newVariableName() : v);
				}
			});
		}
	}
	private Clause standardizedClause(Clause c, HashMap<String, String> table)
	{
		if(isStandardized(c)){
			return getStandardized(c);
		}
		Clause sc = new Clause();
		sc.mPredicate = c.mPredicate;
		c.mVariables.forEach((v) -> {sc.mVariables.add(table.get(v));} );
		addStandardized(sc);
		return sc;
	}
	Rule standardizeVariables(Rule rule)
	{	
		Rule standardized = new Rule();	
		if(rule.mLefts == null){//fact
			standardized.mLefts = new LinkedList<Clause>();
			standardized.mRight = rule.mRight;
			return standardized;
		}
		
		HashMap<String, String> table = new HashMap<String, String>();
		varReplacementTable(rule.mRight, table);
		rule.mLefts.forEach((c) -> {varReplacementTable(c, table);});
		
		standardized.mRight = standardizedClause(rule.mRight, table);
		standardized.mLefts =  new LinkedList<Clause>();
		for(Clause c : rule.mLefts){
			standardized.mLefts.add(standardizedClause(c, table));
		}
		//System.out.println(mStandardizedClause + "\t standardizeVariables\t" +  rule);
		//System.out.println("standardized\t" + standardized);
		return standardized;
	}
}

public class inference 
{
	static List<Clause> readQueries(Scanner scanner, int numQuery)
	{
		List<Clause> qs = new LinkedList<Clause>();
		while(numQuery-->0){
			//System.out.println(scanner.nextLine().split(" => ")[0]);
			qs.add(new Clause(scanner.nextLine()));
		}
		return qs;
	}
	public static void main(String[] args) throws IOException
	{
		String fileName = "output.txt";
		Scanner scanner = new Scanner(new File(args[1]));
		int numQuery = Integer.parseInt(scanner.nextLine());
		List<Clause> queries = readQueries(scanner, numQuery);
		KnowledgeBase kb = new KnowledgeBase();
		int numKB = Integer.parseInt(scanner.nextLine());
		kb.addKB(scanner, numKB);
		BufferedWriter bw =new BufferedWriter(new FileWriter(fileName));
		for(Clause q : queries){
			boolean val = kb.canBeInfered(q);
			bw.write(val? "TRUE" : "FALSE");
			//System.out.println(val);
			bw.newLine();
		}
		bw.close();
		//queries.forEach((q) -> {System.out.println(kb.canBeInfered(q)); bw.write(str);});
		//System.out.println(kb.canBeInfered(queries.remove(1)));
		scanner.close();
	} 
}
