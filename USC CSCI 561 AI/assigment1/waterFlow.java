import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Map.Entry;
import java.util.PriorityQueue;
import java.util.Queue;
import java.util.Set;
import java.util.Stack;
import java.util.TreeMap;
import java.util.concurrent.LinkedBlockingQueue;

public class waterFlow 
{
	private static class DFSNode extends
	Node{
		@Override
		public int compareTo(Node arg0) {
			Node n2 = (Node) arg0;
			return n2.name.compareTo(name);
		}
	}
	private static class Node implements Comparable<Node>{
		String name;
		TreeMap<Node, Edge> adjacencies = new TreeMap<Node, Edge>();
		int time = 1;
		boolean isVisited = false;
		@Override
		public int compareTo(Node arg0) {
			Node n2 = (Node) arg0;
			if(time != n2.time){
				return new Integer(time).compareTo(n2.time);
			}
			return name.compareTo(n2.name);
		}
		static Node getNode(String name){
			return name.equals("DFS")? new DFSNode(): new Node();
		}
	}
	private static class Edge{
		int length = 1;
		boolean []notavailable = new boolean[24];
	}
	private static class SearchStructure{
		String function;
		Node graph;
		Set<String> goals = new HashSet<String>();
		Node doSearch(){
			graph.isVisited = true;
			if(function.equals("BFS")){
				return bfs(graph, goals);
			}
			if(function.equals("DFS")){
				return dfs(graph, goals);
			}
			if(function.equals("UCS")){
				return ucs(graph, goals);
			}
			return null;
		}
	}
	static void addEdgebyLine(Map<String, Node> nodes, String line, String type){
		String[] tokens = line.split(" ");
		String from = tokens[0], to = tokens[1];
		Edge edge = new Edge();
		if(type.equals("UCS")){
			edge.length = Integer.parseInt(tokens[2]);
			int numOffPeriod = Integer.parseInt(tokens[3]);
			for(int i = 0; i < numOffPeriod; ++i){
				String offPeriod = tokens[4+i];
				String[] fromandend = offPeriod.split("-");
				int fromtime = Integer.parseInt(fromandend[0]);
				int endtime = Integer.parseInt(fromandend[1]);
				for(int time = fromtime; time <= endtime; time++){
					edge.notavailable[time%24] = true;
				}
			}
		}
		nodes.get(from).adjacencies.put(nodes.get(to), edge);
	}
	static SearchStructure buildSearchStructure(BufferedReader br) throws IOException{
		SearchStructure res = new SearchStructure();
		res.function = br.readLine();
		Map<String, Node> nodes = new HashMap<String, Node>();		
		res.graph = Node.getNode(res.function);
		res.graph.name = br.readLine();
		nodes.put(res.graph.name, res.graph);
		String goalnames = br.readLine();
		
		String[] names = goalnames.split(" ");
		for(String name : names){
			res.goals.add(name);
			Node node = Node.getNode(res.function);
			node.name = name;
			nodes.put(node.name, node);
		}
		
		String middles = br.readLine();
		String[] middlenames = middles.split(" ");
		for(String name : middlenames){
			Node node = Node.getNode(res.function);
			node.name = name;
			nodes.put(node.name, node);
		}
		int edgeNum = Integer.parseInt(br.readLine()) ;
		while( edgeNum-- > 0 ){
			addEdgebyLine(nodes, br.readLine(), res.function);
		}
		res.graph.time = Integer.parseInt(br.readLine());
		br.readLine();
		return res;
	}
	static Node bfs(Node start, Set<String> goals)
	{
		Queue<Node> queue = new LinkedBlockingQueue<Node>();
		queue.add(start);
		while(!queue.isEmpty()){
			Node cur = queue.remove();
			if(goals.contains(cur.name)){
				return cur;
			}		
			for(Entry<Node, Edge> entry : cur.adjacencies.entrySet()){		
				if(!entry.getKey().isVisited){
					entry.getKey().isVisited = true;
					entry.getKey().time =  cur.time + 1;
					queue.add(entry.getKey());
				}
			}

		}
		return null;
	}
	static Node dfs(Node start, Set<String> goals)
	{	
		Stack<Node> stack = new Stack<Node>();
		stack.add(start);
		
		while(!stack.empty()){
			Node cur = stack.pop();
			System.out.print(cur.name +"->");
			/*if(goals.contains(cur.name)){
				return cur;
			}*/		
			for(Entry<Node, Edge> entry : cur.adjacencies.entrySet()){		
				if(!entry.getKey().isVisited){
					entry.getKey().isVisited = true;
					entry.getKey().time =  cur.time + 1;
					stack.add(entry.getKey());
					if(goals.contains(entry.getKey().name)){
						return entry.getKey();
					}		
				}
			}

		}
		return null;
	}
	
	static Node ucs(Node start, Set<String> goals)
	{
		PriorityQueue<Node> open = new PriorityQueue<Node>();
		open.add(start);
		while(!open.isEmpty() ){
			Node cur = open.remove();
			if(goals.contains(cur.name)){
				return cur;
			}
			for(Entry<Node, Edge> entry : cur.adjacencies.entrySet()){
				Node node = entry.getKey();
				Edge edge = entry.getValue();
				if((!node.isVisited || node.time > cur.time + edge.length) && !edge.notavailable[cur.time%24]){	
					node.isVisited = true;
					if(open.contains(node)){
						open.remove(node);
					}
					node.time = cur.time + edge.length;	
					open.add(node);
				}				
			}
		}
		return null;
	}
	public static void main(String[] args) throws IOException{
		FileInputStream in = new FileInputStream(args[1]);
		FileOutputStream out = new FileOutputStream("output.txt");
		BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(out) );			
		BufferedReader br = new BufferedReader(new InputStreamReader(in));
		int num = Integer.parseInt(br.readLine());
		while(num-- > 0){		
			SearchStructure ss = buildSearchStructure(br);
			Node res = ss.doSearch();
			if(res != null){
				bw.write(res.name + " " + res.time%24);
			}else{
				bw.write("None");
			}
			bw.newLine();	
		}
		if(br != null) br.close();
		if(bw != null) bw.close();
	}
}
