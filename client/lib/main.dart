import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:intl/intl.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:provider/provider.dart';

void main() {
  runApp(const ClientApp());
}

class ClientApp extends StatelessWidget {
  const ClientApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (context) => AuthManager(),
      child: MaterialApp(
        title: 'Flow DB Manager',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(seedColor: Colors.blueGrey),
          useMaterial3: true,
        ),
        home: const AuthWrapper(),
      ),
    );
  }
}

// ==========================================
// 1. DATA MODEL
// ==========================================
class FlowModel {
  final String id;
  final String name;
  final String description;
  final String createdAt;
  final String? startAt;
  final String? endAt;
  final String? ownerId;

  FlowModel({
    required this.id,
    required this.name,
    required this.description,
    required this.createdAt,
    this.startAt,
    this.endAt,
    this.ownerId,
  });

  factory FlowModel.fromJson(Map<String, dynamic> json) {
    return FlowModel(
      id: json['id'] as String,
      name: json['name'] as String,
      description: json['description'] as String,
      createdAt: json['created_at'] as String,
      startAt: json['start_at'] as String?,
      endAt: json['end_at'] as String?,
      ownerId: json['owner_id'] as String?,
    );
  }
}

// ==========================================
// 2. API SERVICE (Connects to C++ Crow Server)
// ==========================================
class ApiService {
  // Note: If testing on Android Emulator, change 127.0.0.1 to 10.0.2.2
  static const String baseUrl = 'http://127.0.0.1:8080/api/v1';

  // ==========================================
  // AUTHENTICATION METHODS
  // ==========================================

  // Register a new user (no email verification as requested)
  Future<Map<String, dynamic>> registerUser({
    required String email,
    required String password,
    required String firstName,
    String? lastName,
    String? nickname,
  }) async {
    final response = await http.post(
      Uri.parse('$baseUrl/auth/register'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'email': email,
        'password': password,
        'first_name': firstName,
        if (lastName != null && lastName.isNotEmpty) 'last_name': lastName,
        if (nickname != null && nickname.isNotEmpty) 'nickname': nickname,
      }),
    );

    if (response.statusCode == 201) {
      return jsonDecode(response.body);
    } else if (response.statusCode == 409) {
      throw Exception('User with this email or nickname already exists');
    } else {
      throw Exception('Registration failed: ${response.body}');
    }
  }

  // Login user and get session token
  Future<Map<String, dynamic>> login(String email, String password) async {
    final response = await http.post(
      Uri.parse('$baseUrl/auth/login'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'email': email,
        'password': password,
      }),
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else {
      throw Exception('Login failed: Invalid email or password');
    }
  }

  // Get current user from session
  Future<Map<String, dynamic>?> getCurrentUser(String token) async {
    final response = await http.get(
      Uri.parse('$baseUrl/auth/me'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else if (response.statusCode == 401) {
      return null; // Invalid or expired token
    } else {
      throw Exception('Failed to get current user');
    }
  }

  // Logout user
  Future<void> logout(String token) async {
    final response = await http.delete(
      Uri.parse('$baseUrl/auth/logout'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to logout');
    }
  }

  // Get session info
  Future<Map<String, dynamic>?> getSessionInfo(String token) async {
    final response = await http.get(
      Uri.parse('$baseUrl/auth/session'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else if (response.statusCode == 401) {
      return null; // Invalid or expired token
    } else {
      throw Exception('Failed to get session info');
    }
  }

  // ==========================================
  // FLOW API METHODS (requires auth)
  // ==========================================

  Future<List<FlowModel>> getFlows(String token) async {
    final response = await http.get(
      Uri.parse('$baseUrl/flows'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );
    if (response.statusCode == 200) {
      List<dynamic> body = jsonDecode(response.body);
      return body.map((item) => FlowModel.fromJson(item)).toList();
    } else {
      throw Exception('Failed to load flows');
    }
  }

  Future<List<FlowModel>> getUserFlows(String token, String userId) async {
    final allFlows = await getFlows(token);
    // Filter flows where current user is the owner
    List<FlowModel> userOwnedFlows = allFlows.where((flow) => flow.ownerId == userId).toList();
    
    // TODO: Add shared flows logic when API supports it
    // For now, only return user's own flows
    return userOwnedFlows;
  }

  Future<void> createFlow(String token, String name, String description, String ownerId, {String? startAt, String? endAt}) async {
    final response = await http.post(
      Uri.parse('$baseUrl/flows'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
      body: jsonEncode({
        'name': name,
        'description': description,
        'owner_id': ownerId,
        if (startAt != null && startAt.isNotEmpty) 'start_at': startAt,
        if (endAt != null && endAt.isNotEmpty) 'end_at': endAt,
      }),
    );
    if (response.statusCode != 201) {
      throw Exception('Failed to create flow: ${response.body}');
    }
  }

  Future<void> updateFlow(String token, String id, String name, String description, String? ownerId, {String? startAt, String? endAt}) async {
    final response = await http.put(
      Uri.parse('$baseUrl/flows/$id'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
      body: jsonEncode({
        'name': name,
        'description': description,
        'owner_id': ownerId,
        if (startAt != null && startAt.isNotEmpty) 'start_at': startAt,
        if (endAt != null && endAt.isNotEmpty) 'end_at': endAt,
      }),
    );
    if (response.statusCode != 200) {
      throw Exception('Failed to update flow: ${response.body}');
    }
  }

  Future<void> deleteFlow(String token, String id) async {
    final response = await http.delete(
      Uri.parse('$baseUrl/flows/$id'),
      headers: {
        'Authorization': 'Bearer $token',
      },
    );
    if (response.statusCode != 204) {
      throw Exception('Failed to delete flow: ${response.body}');
    }
  }
}

// ==========================================
// 3. USER DATA MODEL
// ==========================================
class UserModel {
  final String id;
  final String email;
  final String firstName;
  final String? lastName;
  final String? nickname;

  UserModel({
    required this.id,
    required this.email,
    required this.firstName,
    this.lastName,
    this.nickname,
  });

  factory UserModel.fromJson(Map<String, dynamic> json) {
    return UserModel(
      id: json['id'] as String,
      email: json['email'] as String,
      firstName: (json['first_name'] as String?) ?? '',
      lastName: json['last_name'] as String?,
      nickname: json['nickname'] as String?,
    );
  }
}

// ==========================================
// 4. AUTH STATE MANAGER
// ==========================================
class AuthManager extends ChangeNotifier {
  String? _token;
  UserModel? _user;
  bool _isAuthenticated = false;
  final ApiService _apiService = ApiService();

  String? get token => _token;
  UserModel? get user => _user;
  bool get isAuthenticated => _isAuthenticated;

  // Load saved auth state on initialization
  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    _token = prefs.getString('token');
    
    if (_token != null) {
      final userMap = await _apiService.getCurrentUser(_token!);
      _user = userMap != null ? UserModel.fromJson(userMap) : null;
      _isAuthenticated = _user != null;
      notifyListeners();
    }
  }

  // Login user
  Future<bool> login(String email, String password) async {
    try {
      final response = await _apiService.login(email, password);
      _token = response['token'];
      _user = UserModel(
        id: response['user_id'],
        email: response['email'],
        firstName: response['first_name'] ?? '',
        lastName: response['last_name'],
        nickname: response['nickname'],
      );
      _isAuthenticated = true;
      
      // Save to SharedPreferences
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('token', _token!);
      
      debugPrint('Login successful, token: $_token, user: $_user');
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Login failed: $e');
      return false;
    }
  }

  // Register user
  Future<bool> register(String email, String password, String firstName,
      {String? lastName, String? nickname}) async {
    try {
      await _apiService.registerUser(
        email: email,
        password: password,
        firstName: firstName,
        lastName: lastName,
        nickname: nickname,
      );
      
      // After registration, login automatically
      return await login(email, password);
    } catch (e) {
      return false;
    }
  }

  // Logout user
  Future<void> logout() async {
    if (_token != null) {
      try {
        await _apiService.logout(_token!);
      } catch (e) {
        // Ignore logout errors
      }
    }
    
    _token = null;
    _user = null;
    _isAuthenticated = false;
    
    // Clear saved auth state
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('token');
    
    notifyListeners();
  }

  // Check if user is authenticated
  Future<bool> checkAuth() async {
    if (_token == null) {
      _isAuthenticated = false;
      return false;
    }
    
    final userMap = await _apiService.getCurrentUser(_token!);
    if (userMap != null) {
      _user = UserModel.fromJson(userMap);
      _isAuthenticated = true;
      return true;
    } else {
      _token = null;
      _isAuthenticated = false;
      return false;
    }
  }
}

// ==========================================
// 5. AUTHENTICATION SCREENS
// ==========================================
class AuthWrapper extends StatefulWidget {
  const AuthWrapper({super.key});

  @override
  State<AuthWrapper> createState() => _AuthWrapperState();
}

class _AuthWrapperState extends State<AuthWrapper> {
  bool _isCheckingAuth = true;

  @override
  void initState() {
    super.initState();
    _checkInitialAuth();
  }

  Future<void> _checkInitialAuth() async {
    final authManager = context.read<AuthManager>();
    await authManager.init();
    setState(() {
      _isCheckingAuth = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (_isCheckingAuth) {
      return const Scaffold(
        body: Center(
          child: CircularProgressIndicator(),
        ),
      );
    }

    // Listen to auth changes using Provider
    final authManager = context.watch<AuthManager>();
    
    debugPrint('AuthManager state - token: ${authManager.token}, user: ${authManager.user}, isAuthenticated: ${authManager.isAuthenticated}');
    
    return Scaffold(
      body: Center(
        child: authManager.isAuthenticated && authManager.user != null
            ? FlowListScreen(authManager: authManager)
            : LoginScreen(authManager: authManager),
      ),
    );
  }
}

class LoginScreen extends StatefulWidget {
  final AuthManager authManager;

  const LoginScreen({super.key, required this.authManager});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final _formKey = GlobalKey<FormState>();
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  bool _isLogin = true;
  String? _errorMessage;

  @override
  void dispose() {
    _emailController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _submitForm() async {
    if (!_formKey.currentState!.validate()) return;

    setState(() {
      _errorMessage = null;
    });

    bool success;
    if (_isLogin) {
      success = await widget.authManager.login(
        _emailController.text.trim(),
        _passwordController.text,
      );
    } else {
      success = await widget.authManager.register(
        _emailController.text.trim(),
        _passwordController.text,
        _emailController.text.split('@')[0], // Use email prefix as first name
      );
    }

    if (!success && mounted) {
      setState(() {
        _errorMessage = _isLogin
            ? 'Invalid email or password'
            : 'Registration failed. Please try a different email or nickname.';
      });
      debugPrint('Login/Registration failed');
    } else if (mounted) {
      // Navigate to flow list on success
      debugPrint('Login/Registration successful, navigating to FlowListScreen');
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(builder: (context) => FlowListScreen(authManager: widget.authManager)),
      );
    }
    // After successful login, AuthManager.notifyListeners() is called
    // and context.watch will rebuild AuthWrapper to show FlowListScreen
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Form(
          key: _formKey,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const SizedBox(height: 48),
              Icon(
                _isLogin ? Icons.login : Icons.person_add,
                size: 80,
                color: Theme.of(context).colorScheme.primary,
              ),
              const SizedBox(height: 24),
              Text(
                _isLogin ? 'Login' : 'Register',
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 24),
              if (_errorMessage != null)
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.red.shade50,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.red.shade200),
                  ),
                  child: Row(
                    children: [
                      Icon(Icons.error, color: Colors.red.shade700),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          _errorMessage!,
                          style: TextStyle(color: Colors.red.shade700),
                        ),
                      ),
                    ],
                  ),
                ),
              const SizedBox(height: 24),
              TextFormField(
                controller: _emailController,
                decoration: const InputDecoration(
                  labelText: 'Email',
                  prefixIcon: Icon(Icons.email),
                ),
                keyboardType: TextInputType.emailAddress,
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Please enter your email';
                  }
                  if (!value.contains('@')) {
                    return 'Please enter a valid email';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: _passwordController,
                decoration: const InputDecoration(
                  labelText: 'Password',
                  prefixIcon: Icon(Icons.lock),
                ),
                obscureText: true,
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Please enter your password';
                  }
                  if (value.length < 6) {
                    return 'Password must be at least 6 characters';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 24),
              ElevatedButton(
                onPressed: _submitForm,
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  minimumSize: const Size(double.infinity, 0),
                ),
                child: Text(_isLogin ? 'Login' : 'Register'),
              ),
              const SizedBox(height: 16),
              TextButton(
                onPressed: () {
                  setState(() {
                    _isLogin = !_isLogin;
                    _errorMessage = null;
                    _passwordController.clear();
                  });
                },
                child: Text(
                  _isLogin
                      ? "Don't have an account? Register"
                      : 'Already have an account? Login',
                ),
              ),
              if (_isLogin) ...[
                const SizedBox(height: 16),
                OutlinedButton.icon(
                  onPressed: () async {
                    await widget.authManager.logout();
                  },
                  icon: const Icon(Icons.logout),
                  label: const Text('Logout'),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

// ==========================================
// 6. USER INTERFACE
// ==========================================
class FlowListScreen extends StatefulWidget {
  final AuthManager authManager;

  const FlowListScreen({super.key, required this.authManager});

  @override
  State<FlowListScreen> createState() => _FlowListScreenState();
}

class FlowDialog extends StatefulWidget {
  final bool isEditing;
  final TextEditingController nameController;
  final TextEditingController descController;
  final DateTime? initialStartDate;
  final DateTime? initialEndDate;
  final Function(String, String, String?, String?) onSave;
  final Function(String) onError;

  const FlowDialog({
    super.key,
    required this.isEditing,
    required this.nameController,
    required this.descController,
    this.initialStartDate,
    this.initialEndDate,
    required this.onSave,
    required this.onError,
  });

  @override
  State<FlowDialog> createState() => _FlowDialogState();
}

class _FlowDialogState extends State<FlowDialog> {
  DateTime? selectedStartDate;
  DateTime? selectedEndDate;

  @override
  void initState() {
    super.initState();
    selectedStartDate = widget.initialStartDate;
    selectedEndDate = widget.initialEndDate;
  }

  @override
  void dispose() {
    widget.nameController.dispose();
    widget.descController.dispose();
    super.dispose();
  }

  String _formatDateTime(DateTime dt) {
    final formatter = DateFormat('yyyy-MM-dd HH:mm');
    return formatter.format(dt);
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(widget.isEditing ? 'Edit Flow' : 'New Flow'),
      content: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: widget.nameController,
              decoration: const InputDecoration(labelText: 'Name'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: widget.descController,
              decoration: const InputDecoration(labelText: 'Description'),
            ),
            const SizedBox(height: 16),
            // Start datetime picker
            ListTile(
              leading: const Icon(Icons.calendar_today),
              title: Text(
                selectedStartDate != null
                    ? _formatDateTime(selectedStartDate!)
                    : 'Select start datetime (optional)',
              ),
              trailing: const Icon(Icons.chevron_right),
              dense: true,
              onTap: () async {
                final DateTime? picked = await showDatePicker(
                  context: context,
                  firstDate: DateTime(2000),
                  lastDate: DateTime(2100),
                  initialDate: selectedStartDate ?? DateTime.now(),
                );
                if (picked != null) {
                  final TimeOfDay? time = await showTimePicker(
                    context: context,
                    initialTime: TimeOfDay.fromDateTime(picked),
                  );
                  if (time != null) {
                    final combined = DateTime(
                      picked.year,
                      picked.month,
                      picked.day,
                      time.hour,
                      time.minute,
                    );
                    setState(() {
                      selectedStartDate = combined;
                    });
                  }
                }
              },
            ),
            // End datetime picker
            ListTile(
              leading: const Icon(Icons.calendar_today),
              title: Text(
                selectedEndDate != null
                    ? _formatDateTime(selectedEndDate!)
                    : 'Select end datetime (optional)',
              ),
              trailing: const Icon(Icons.chevron_right),
              dense: true,
              onTap: () async {
                final DateTime? picked = await showDatePicker(
                  context: context,
                  firstDate: DateTime(2000),
                  lastDate: DateTime(2100),
                  initialDate: selectedEndDate ?? DateTime.now(),
                );
                if (picked != null) {
                  final TimeOfDay? time = await showTimePicker(
                    context: context,
                    initialTime: TimeOfDay.fromDateTime(picked),
                  );
                  if (time != null) {
                    final combined = DateTime(
                      picked.year,
                      picked.month,
                      picked.day,
                      time.hour,
                      time.minute,
                    );
                    setState(() {
                      selectedEndDate = combined;
                    });
                  }
                }
              },
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: () async {
            final name = widget.nameController.text.trim();
            final desc = widget.descController.text.trim();
            if (name.isEmpty || desc.isEmpty) return;

            // Validate: end_at can't be earlier than start_at but can be equal
            if (selectedStartDate != null && selectedEndDate != null) {
              if (selectedEndDate!.isBefore(selectedStartDate!)) {
                widget.onError('Error: End datetime must be the same or after start datetime');
                return;
              }
            }

            // Format dates for API (ISO 8601)
            final startAtStr = selectedStartDate != null
                ? selectedStartDate!.toIso8601String()
                : '';
            final endAtStr = selectedEndDate != null
                ? selectedEndDate!.toIso8601String()
                : '';

            Navigator.pop(context); // Close dialog

            try {
              await widget.onSave(name, desc, startAtStr.isNotEmpty ? startAtStr : null, endAtStr.isNotEmpty ? endAtStr : null);
            } catch (e) {
              widget.onError('Error: $e');
            }
          },
          child: const Text('Save'),
        ),
      ],
    );
  }
}

class _FlowListScreenState extends State<FlowListScreen> {
  final ApiService _apiService = ApiService();
  late Future<List<FlowModel>> _flowsFuture;

  String? _getToken() {
    return widget.authManager.token;
  }

  String? _getUserId() {
    return widget.authManager.user?.id;
  }

  @override
  void initState() {
    super.initState();
    _refreshFlows();
  }

  void _refreshFlows() {
    setState(() {
      final token = _getToken();
      final userId = _getUserId();
      if (token != null && userId != null) {
        _flowsFuture = _apiService.getUserFlows(token, userId);
      } else {
        _flowsFuture = Future.value([]);
      }
    });
  }

  // Dialog for both CREATE and UPDATE
  void _showFlowDialog({FlowModel? flow}) {
    final isEditing = flow != null;
    final nameController = TextEditingController(text: flow?.name ?? '');
    final descController = TextEditingController(text: flow?.description ?? '');
    DateTime? selectedStartDate;
    DateTime? selectedEndDate;

    // Parse existing dates if editing
    if (isEditing) {
      if (flow.startAt != null && flow.startAt!.isNotEmpty) {
        selectedStartDate = DateTime.parse(flow.startAt!);
      }
      if (flow.endAt != null && flow.endAt!.isNotEmpty) {
        selectedEndDate = DateTime.parse(flow.endAt!);
      }
    }

    showDialog(
      context: context,
      builder: (dialogContext) => FlowDialog(
        isEditing: isEditing,
        nameController: nameController,
        descController: descController,
        initialStartDate: selectedStartDate,
        initialEndDate: selectedEndDate,
        onSave: (name, desc, startAt, endAt) async {
          final token = _getToken()!;
          final ownerId = widget.authManager.user!.id;
          if (isEditing) {
            await _apiService.updateFlow(token, flow.id, name, desc, ownerId, startAt: startAt, endAt: endAt);
          } else {
            await _apiService.createFlow(token, name, desc, ownerId, startAt: startAt, endAt: endAt);
          }
          _refreshFlows();
        },
        onError: (message) {
          ScaffoldMessenger.of(dialogContext).showSnackBar(SnackBar(content: Text(message)));
        },
      ),
    );
  }

  String _formatDateTime(DateTime dt) {
    final formatter = DateFormat('yyyy-MM-dd HH:mm');
    return formatter.format(dt);
  }

  @override
  Widget build(BuildContext context) {
    final user = widget.authManager.user;
    
    return Scaffold(
      appBar: AppBar(
        title: const Text('PostgreSQL Flows Editor'),
        actions: [
          PopupMenuButton<String>(
            onSelected: (String action) async {
              if (action == 'logout') {
                await widget.authManager.logout();
                // Navigate to login screen
                Navigator.of(context).pushReplacement(
                  MaterialPageRoute(builder: (context) => LoginScreen(authManager: widget.authManager)),
                );
              }
            },
            itemBuilder: (BuildContext context) => <PopupMenuEntry<String>>[
              if (user != null)
                PopupMenuItem<String>(
                  value: 'profile',
                  child: ListTile(
                    leading: const Icon(Icons.person),
                    title: Text(user.email),
                    dense: true,
                  ),
                ),
              const PopupMenuDivider(),
              const PopupMenuItem<String>(
                value: 'logout',
                child: ListTile(
                  leading: Icon(Icons.logout),
                  title: Text('Logout'),
                ),
              ),
            ],
          ),
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _refreshFlows,
            tooltip: 'Refresh Data',
          ),
        ],
      ),
      body: FutureBuilder<List<FlowModel>>(
        future: _flowsFuture,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          } else if (snapshot.hasError) {
            return Center(child: Text('Error: ${snapshot.error}'));
          } else if (!snapshot.hasData || snapshot.data!.isEmpty) {
            return const Center(child: Text('No rows found. Click + to add one.'));
          }

          final flows = snapshot.data!;
          return ListView.separated(
            padding: const EdgeInsets.all(16),
            itemCount: flows.length,
            separatorBuilder: (_, _) => const Divider(),
            itemBuilder: (context, index) {
              final flow = flows[index];
              return ListTile(
                title: Text(flow.name, style: const TextStyle(fontWeight: FontWeight.bold)),
                subtitle: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(flow.description),
                    Text('ID: ${flow.id}'),
                    if (flow.startAt != null && flow.startAt!.isNotEmpty)
                      Text('Start: ${_formatDateTime(DateTime.parse(flow.startAt!))}'),
                    if (flow.endAt != null && flow.endAt!.isNotEmpty)
                      Text('End: ${_formatDateTime(DateTime.parse(flow.endAt!))}'),
                  ],
                ),
                isThreeLine: true,
                trailing: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    IconButton(
                      icon: const Icon(Icons.edit, color: Colors.blue),
                      onPressed: () => _showFlowDialog(flow: flow),
                    ),
                    IconButton(
                      icon: const Icon(Icons.delete, color: Colors.red),
                      onPressed: () async {
                        await _apiService.deleteFlow(_getToken()!, flow.id);
                        _refreshFlows();
                      },
                    ),
                  ],
                ),
              );
            },
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showFlowDialog(),
        child: const Icon(Icons.add),
      ),
    );
  }
}
